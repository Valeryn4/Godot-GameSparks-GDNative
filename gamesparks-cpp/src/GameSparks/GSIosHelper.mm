#ifdef __OBJC__
#include <GameSparks/gsstl.h>
#include <GameSparks/GSPlatformDeduction.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/utsname.h>
//#include <iostream>
//#include <thread>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if GS_TARGET_PLATFORM == GS_PLATFORM_IOS
#	import <UIKit/UIKit.h>
#	import <UIKit/UIDevice.h>

	gsstl::string gs_ios_get_writeable_base_path()
	{
	    NSString *libraryPath = [NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSUserDomainMask, YES) lastObject];

	    return [libraryPath UTF8String];
	}


	gsstl::string get_ios_device_id()
	{
		static gsstl::string device_id;

		if(device_id.empty())
		{
			#if (TARGET_OS_SIMULATOR)
				// when running the test-suite in the iOS simulator from the command line via xcrun simctl
				// the test hangs in [UIDevice identifierForVendor]. So we just return a hard-coded device id
				// in the simulator for now.
				device_id = "IOS-SIMULATOR-DEVICE-ID";
			#else
				UIDevice *device = [UIDevice currentDevice];
			    #pragma clang diagnostic ignored "-Wundeclared-selector"
			    if ([UIDevice instancesRespondToSelector:@selector(identifierForVendor)]) {
			        device_id = [[[device identifierForVendor] UUIDString] UTF8String];
			    } else {
			        #pragma clang diagnostic push
			        #pragma clang diagnostic ignored "-Wdeprecated-declarations"
			        device_id = [[device performSelector:@selector(uniqueIdentifier)] UTF8String];
			    
			    }
			    #pragma clang diagnostic pop
			#endif
		}

	    return device_id;
	}

    namespace
    {
        static gsstl::string getDisplayResolution()
        {
            @autoreleasepool
            {
                CGRect screenRect = [[UIScreen mainScreen] bounds];
                CGFloat screenScale = [[UIScreen mainScreen] scale];
                return [[NSString stringWithFormat:@"%.0fx%.0f",screenRect.size.width * screenScale, screenRect.size.height * screenScale]  UTF8String];
            }
        }
    }

    gsstl::map<gsstl::string, gsstl::string> gs_get_device_stats()
    {
        gsstl::map<gsstl::string, gsstl::string> ret;

        @autoreleasepool{
            struct utsname systemInfo;
            uname(&systemInfo);
            ret["model"]        = systemInfo.machine;
            
            ret["cpu.vendor"]   = "ARM";
            ret["cpu.cores"]    = gsstl::to_string(gsstl::thread::hardware_concurrency() );
            ret["memory"]       = gsstl::to_string([NSProcessInfo processInfo].physicalMemory / 1024 / 1024) + " MB";
            ret["os.name"]      = [[[UIDevice currentDevice] systemName] UTF8String];
            ret["os.version"]   = [[[UIDevice currentDevice] systemVersion] UTF8String];
            ret["resolution"]   = getDisplayResolution();
            ret["manufacturer"] = "Apple";
        }
        return ret;
    }

#elif GS_TARGET_PLATFORM == GS_PLATFORM_MAC
#	import <Foundation/Foundation.h>
#   import <AppKit/AppKit.h>

	gsstl::string get_osx_device_id()
	{
		static gsstl::string device_id;

		if (device_id.empty())
		{
			NSArray * args = @[@"-rd1", @"-c", @"IOPlatformExpertDevice", @"|", @"grep", @"model"];
			NSTask * task = [NSTask new];
			[task setLaunchPath:@"/usr/sbin/ioreg"];
			[task setArguments:args];

			NSPipe * pipe = [NSPipe new];
			[task setStandardOutput:pipe];
			[task launch];

			NSArray * args2 = @[@"/IOPlatformUUID/ { split($0, line, \"\\\"\"); printf(\"%s\\n\", line[4]); }"];
			NSTask * task2 = [NSTask new];
			[task2 setLaunchPath:@"/usr/bin/awk"];
			[task2 setArguments:args2];

			NSPipe * pipe2 = [NSPipe new];
			[task2 setStandardInput:pipe];
			[task2 setStandardOutput:pipe2];
			NSFileHandle * fileHandle2 = [pipe2 fileHandleForReading];
			[task2 launch];

			NSData * data = [fileHandle2 readDataToEndOfFile];
			NSString * uuid = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];

			device_id = [uuid UTF8String];
		}

		return device_id;
	}


namespace
{
    static gsstl::string sysctlbyname_string(const gsstl::string name)
    {
        size_t length;
        gsstl::string hw_model;
        ::sysctlbyname(name.c_str(), NULL, &length, NULL, 0);
        if (length)
        {
            gsstl::vector<char> hw_string_(length, 0);
            ::sysctlbyname(name.c_str(), hw_string_.data(), &length, NULL, 0);
            gsstl::copy(hw_string_.begin(), hw_string_.end() - 1, gsstl::back_inserter(hw_model));
        }
        return hw_model;
    }
    
    template <typename T>
    static T sysctlbyname_generic(const gsstl::string name)
    {
        T ret=0;
        size_t length = sizeof(T);
        auto result = ::sysctlbyname(name.c_str(), &ret, &length, NULL, 0);
        (void)result;
        assert(result == 0);
        assert( length == sizeof(T) );
        return ret;
    }
    
    static gsstl::string getMacOSName()
    {
        @autoreleasepool
        {
            NSDictionary* dict = [NSDictionary dictionaryWithContentsOfFile: @"/System/Library/CoreServices/SystemVersion.plist"];
            return [[dict objectForKey: @"ProductName"] UTF8String];
        }
    }
    
    static gsstl::string getMacOSVersion()
    {
        @autoreleasepool
        {
            NSDictionary* dict = [NSDictionary dictionaryWithContentsOfFile: @"/System/Library/CoreServices/SystemVersion.plist"];
            return [[dict objectForKey: @"ProductVersion"] UTF8String];
        }
    }
    
    static gsstl::string getDisplayResolution()
    {
        @autoreleasepool
        {
            int width = [[NSScreen mainScreen] frame].size.width;
            int height = [[NSScreen mainScreen] frame].size.height;
            
            return [[NSString stringWithFormat:@"%dx%d", width, height]  UTF8String];
        }
    }
}

gsstl::map<gsstl::string, gsstl::string> gs_get_device_stats()
{
    gsstl::map<gsstl::string, gsstl::string> ret;
    
    ret["cpu.vendor"]   = sysctlbyname_string("machdep.cpu.brand_string");
    //ret["cpu.cores"]    = std::to_string(sysctlbyname_generic<uint32_t>("hw.logicalcpu"));
    ret["cpu.cores"]    = gsstl::to_string(gsstl::thread::hardware_concurrency() );
    ret["memory"]       = gsstl::to_string(sysctlbyname_generic<uint64_t>("hw.memsize") / 1024 / 1024) + " MB";
    ret["model"]        = sysctlbyname_string("hw.model");
    ret["os.name"]      = getMacOSName();
    ret["os.version"]   = getMacOSVersion();
    ret["resolution"]   = getDisplayResolution();
    ret["manufacturer"] = "Apple";
    
    return ret;
}

#endif


#endif /* #ifdef __OBJC__ */
