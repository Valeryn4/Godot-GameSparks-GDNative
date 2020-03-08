#ifndef GAMESPAKS_CERTIFICATE_STORE_HPP
#define GAMESPAKS_CERTIFICATE_STORE_HPP

#include "GameSparks/GSPlatformDeduction.h"

#if defined(WIN32)
#	include <Windows.h>
#	if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#		include <Wincrypt.h>
#		pragma comment(lib, "Crypt32.lib")
#	endif
#elif (GS_TARGET_PLATFORM == GS_PLATFORM_PS4)
extern "C"
{
	extern int orbis_check_certificates(mbedtls_ssl_config& conf);
}
#elif (GS_TARGET_PLATFORM == GS_PLATFORM_ANDROID)
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/net.h>
#include <mbedtls/error.h>
#include <mbedtls/platform.h>
#include <mbedtls/debug.h>

#if defined(__UNREAL__)
//#include "../../../Core/Public/Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#include "Android/AndroidApplication.h"
#include <android/log.h>

#define LOG_TAG		"main"
#define LOGD(...)	__android_log_print(ANDROID_LOG_DEBUG,LOG_TAG,__VA_ARGS__)
#define LOGE(...)	__android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#endif
#endif

#if !defined(IW_SDK)
#	include <mutex>
#endif

namespace GameSparks { namespace Util {
	class CertificateStore
	{
		public:
#if (GS_TARGET_PLATFORM == GS_PLATFORM_ANDROID)
			static int numCerts;
			static unsigned char *pCertsEncoded;
			static int *pCertsLength;
#endif

			/// an implementation is expected to call mbedtls_ssl_conf_ca_chain() with CA certs and CRLs
			/// retrieved from the OS. After that it is expected to call mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);
			/// if the implementation does not do that, the SSL/TLS certificate from the server are not validated.
			/// currently this is only implemented on Windows.
			static int setup(mbedtls_ssl_config& conf)
			{
                (void)conf;

				#if defined(WIN32)
                #if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
					// Wincrypt appears to be not implemented for non-desktop apps
					static bool initialized = false;
					static char errormsg[256];
					static mbedtls_x509_crt cacert;
					static mbedtls_x509_crl crl;
					static gsstl::mutex mutex;
					gsstl::lock_guard<gsstl::mutex> guard(mutex);

					if (!initialized)
					{
						mbedtls_x509_crt_init(&cacert);
						mbedtls_x509_crl_init(&crl);

						static const char* store_names[] = {
							"ROOT",
							"CA"
						};

						for (int i = 0; i != sizeof(store_names) / sizeof(store_names[0]); ++i)
						{
							//HCERTSTORE cs = CertOpenSystemStore(0, store_names[i]);
							HCERTSTORE cs = CertOpenStore(
								CERT_STORE_PROV_SYSTEM_A,
								X509_ASN_ENCODING,
								NULL,
								CERT_STORE_OPEN_EXISTING_FLAG|
								CERT_STORE_READONLY_FLAG|
								CERT_STORE_SHARE_CONTEXT_FLAG|
								CERT_SYSTEM_STORE_CURRENT_USER,
								store_names[i]
							);
							assert(cs);
							if (!cs)
							{
								return -1;
							}

							for (
								PCCERT_CONTEXT ctx = CertEnumCertificatesInStore(cs, NULL);
								ctx != NULL;
								ctx = CertEnumCertificatesInStore(cs, ctx)
								)
							{
								int res = mbedtls_x509_crt_parse(&cacert, ctx->pbCertEncoded, ctx->cbCertEncoded);
						
								if (res != 0)
								{
									mbedtls_strerror(res, errormsg, sizeof(errormsg));
									mbedtls_fprintf(stderr, "%s\n", errormsg);
								}
							}

							for (
								PCCRL_CONTEXT ctx = CertEnumCRLsInStore(cs, NULL);
								ctx != NULL;
								ctx = CertEnumCRLsInStore(cs, ctx)
								)
							{
								int res = mbedtls_x509_crl_parse(&crl, ctx->pbCrlEncoded, ctx->cbCrlEncoded);

								if (res != 0)
								{
									mbedtls_strerror(res, errormsg, sizeof(errormsg));
									mbedtls_fprintf(stderr, "%s\n", errormsg);
								}
							}

							CertCloseStore(cs, 0);
						}

						initialized = true;
					}

					mbedtls_ssl_conf_ca_chain(&conf, &cacert, &crl);
					mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);
                #endif
				#elif (GS_TARGET_PLATFORM == GS_PLATFORM_PS4)
					int ret;
					static gsslt::mutex mutex;
					gsstl::lock_guard<gsstl::mutex> guard(mutex);

					ret = orbis_check_certificates(conf);

					if (ret < 0) {
						return ret;
					}
				#elif (GS_TARGET_PLATFORM == GS_PLATFORM_ANDROID)
					int totalLength = 0;
					static bool initialized = false;
					static char errormsg[256];
					static mbedtls_x509_crt cacert;
					static mbedtls_x509_crl crl;
					static gsstl::mutex mutex;
					gsstl::lock_guard<gsstl::mutex> guard(mutex);

					if (!initialized)
					{
#if defined(__UNREAL__)
						if (!pCertsEncoded && !pCertsLength) {
							JNIEnv *env = FAndroidApplication::GetJavaEnv();

							if (!env) {
								return JNI_ERR;
							}
							
							jclass cls = FAndroidApplication::FindJavaClass("com/epicgames/ue4/GameActivity");

							if (!cls) {
								LOGE("Could not find the GameActivity class.");
							} else {
								jmethodID methodId = env->GetStaticMethodID(cls, "getCertificates", "()V");
								
								if (!methodId) {
									LOGE("Could not find the getCertificates method.");
								} else {		
									jbyte *_pCertsEncoded;
									jint *_pCertsLength;
									
									env->CallStaticVoidMethod(cls, methodId);

									jfieldID numCertsId = env->GetStaticFieldID(cls, "numCerts", "I");
									jfieldID certsEncodedId = env->GetStaticFieldID(cls, "certsEncoded", "[B");
									jfieldID certsLengthId = env->GetStaticFieldID(cls, "certsLength", "[I");

									jint _numCerts = env->GetStaticIntField(cls, numCertsId);
									jobject certsEncodedObj = env->GetStaticObjectField(cls, certsEncodedId);
									jobject certsLengthObj = env->GetStaticObjectField(cls, certsLengthId);

									jbyteArray *pCertsEncodedArray = reinterpret_cast<jbyteArray*>(&certsEncodedObj);
									jintArray *pCertsLengthArray = reinterpret_cast<jintArray*>(&certsLengthObj);

									if (_numCerts > 0) {
										_pCertsLength = (jint *)malloc(sizeof(jint) * _numCerts);

										env->GetIntArrayRegion(*pCertsLengthArray, 0, _numCerts, _pCertsLength);

										for (int i = 0; i < _numCerts; i++) {
											totalLength += _pCertsLength[i];
										}

										_pCertsEncoded = (jbyte *)malloc(sizeof(jbyte) * totalLength);

										env->GetByteArrayRegion(*pCertsEncodedArray, 0, totalLength, _pCertsEncoded);

										numCerts = _numCerts;
										pCertsEncoded = (unsigned char *)_pCertsEncoded;
										pCertsLength = _pCertsLength;
									}
									
									totalLength = 0;
								}
							}
						}
#endif		
						if (pCertsEncoded && pCertsLength) {
							mbedtls_x509_crt_init(&cacert);
							mbedtls_x509_crl_init(&crl);

							for (int i = 0; i < numCerts; ++i)
							{
								int length = pCertsLength[i];

								if (length > 0)
								{
									int res = mbedtls_x509_crt_parse(&cacert, &(pCertsEncoded[totalLength]), (size_t)length);

									if (res != 0)
									{
										mbedtls_strerror(res, errormsg, sizeof(errormsg));
										mbedtls_fprintf(stderr, "%s\n", errormsg);
									}
								}

								totalLength += length;
							}

							numCerts = 0;

							free(pCertsEncoded);
							pCertsEncoded = NULL;

							free(pCertsLength);
							pCertsLength = NULL;

							initialized = true;
						}
					}

					mbedtls_ssl_conf_ca_chain(&conf, &cacert, &crl);
					mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);
				#endif

				return 0;
			}
	};

}} /* GameSparks::Util */

#endif /* GAMESPAKS_CERTIFICATE_STORE_HPP */
