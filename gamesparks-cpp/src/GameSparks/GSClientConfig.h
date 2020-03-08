// Copyright 2017 GameSparks Ltd 2015, Inc. All Rights Reserved.

#ifndef GSClientConfig_h__
#define GSClientConfig_h__

#include <GameSparks/gsstl.h>
#include <GameSparks/GSObject.h>
#include <GameSparks/GSTime.h>

#include <cmath>

#define DEFAULT_RETRY_BASE  2.0f
#define DEFAULT_RETRY_MAX 60.0f
#define DEFAULT_REQUEST_TIMEOUT 15.0f
#define DEFAULT_DURABLE_CONCURRENT_REQUESTS 1
#define DEFAULT_DURABLE_DRAIN_INTERVAL 0.1f
#define DEFAULT_HANDSHAKE_OFFSET 2.0f
#define MILLIS 1000.0f

// if somehow the windows header got pulled in without NOMINMAX: being defined, min and max get defined as macros
#if defined(min)
#   undef min
#endif

namespace GameSparks{ namespace Core {


        /*!
         * ClientConfig stores settings that can be set by the server.
         * */
		class GSClientConfig
		{
			public:
                /// Singleton accessor
                static GSClientConfig& instance()
                {
                    static GSClientConfig instance;
                    return instance;
                }

                /// serializes the state into a json string
				gsstl::string serialize() const
                {
                    GSRequestData obj;

                    static const auto ms = 1000.0f;

                    obj.AddNumber("retryBase"                , retryBase * ms);
                    obj.AddNumber("retryMax"                 , retryMax * ms);
                    obj.AddNumber("requestTimeout"           , requestTimeout * ms);
                    obj.AddNumber("durableConcurrentRequests", durableConcurrentRequests);
                    obj.AddNumber("durableDrainInterval"     , durableDrainInterval * ms);
                    obj.AddNumber("handshakeOffset"          , handshakeOffset * ms);

                    return obj.GetJSON();
                }

                /// Restores the state from the json representation obtained by calling serialize()
                /// If a value is not present in the passed json object or *invalid*, it is set to the default value
				void deserialze(const gsstl::string& json)
                {
                    const auto obj = GSObject::FromJSON(json);
                    setFromObject(obj);
                }

            
                void setFromObject(const GSData& obj)
                {
                    retryBase = obj.GetFloat("retryBase").GetValueOrDefault(DEFAULT_RETRY_BASE * MILLIS) / MILLIS;
                    if (retryBase <= 0.0) retryBase = DEFAULT_RETRY_BASE;

                    retryMax = obj.GetFloat("retryMax").GetValueOrDefault(DEFAULT_RETRY_MAX * MILLIS) / MILLIS;
                    if (retryMax <= 0.0) retryMax = DEFAULT_RETRY_MAX;

                    requestTimeout = obj.GetFloat("requestTimeout").GetValueOrDefault(DEFAULT_REQUEST_TIMEOUT * MILLIS) / MILLIS;
                    if (requestTimeout <= 0.0) requestTimeout = DEFAULT_REQUEST_TIMEOUT;

                    durableConcurrentRequests = obj.GetInt("durableConcurrentRequests").GetValueOrDefault(DEFAULT_DURABLE_CONCURRENT_REQUESTS);
                    if (durableConcurrentRequests <= 0.0) durableConcurrentRequests = DEFAULT_DURABLE_CONCURRENT_REQUESTS;

                    durableDrainInterval = obj.GetFloat("durableDrainInterval").GetValueOrDefault(DEFAULT_DURABLE_DRAIN_INTERVAL * MILLIS) / MILLIS;
                    if (durableDrainInterval <= 0.0) durableDrainInterval = DEFAULT_DURABLE_DRAIN_INTERVAL;

                    handshakeOffset = obj.GetFloat("handshakeOffset").GetValueOrDefault(DEFAULT_HANDSHAKE_OFFSET * MILLIS) / MILLIS;
                    if (handshakeOffset <= 0.0) handshakeOffset = DEFAULT_HANDSHAKE_OFFSET;
                }

                Seconds ComputeSleepPeriod(int attempt)
                {
                    return rng.getNextFloat(0.0f, gsstl::min(retryMax, retryBase * gsstl::pow(2.0f, float(attempt))));
                }

                Seconds  getRetryBase                () const { return retryBase; }
                Seconds  getRetryMax                 () const { return retryMax; }
                Seconds  getRequestTimeout           () const { return requestTimeout; }
                int      getDurableConcurrentRequests() const { return durableConcurrentRequests; }
                Seconds  getDurableDrainInterval     () const { return durableDrainInterval; }
                Seconds  getHandshakeOffset          () const { return handshakeOffset; }
            private:
                GSClientConfig()
                :rng(unsigned(clock()))
                {
                    retryBase = DEFAULT_RETRY_BASE;
                    retryMax = DEFAULT_RETRY_MAX;
                    requestTimeout = DEFAULT_REQUEST_TIMEOUT;
                    durableConcurrentRequests = DEFAULT_DURABLE_CONCURRENT_REQUESTS;
                    durableDrainInterval = DEFAULT_DURABLE_DRAIN_INTERVAL;
                    handshakeOffset = DEFAULT_HANDSHAKE_OFFSET;

                }

                // simple linear congruential pseudo random number generator to avoid calling the system RNG and mess up the seed of client code
                class RNG
                {
                    public:
                        RNG(unsigned seed=1):x(seed){}

                        // maps the unsigned integer x which has to be between rmin and rmax to a float in the interval [a, b)
                        float getNextFloat(const float a=0.0f, const float b=1.0f)
                        {
                            x = generate(); // update internal state
                            const float fx = (x - rmin) / (rmax - rmin + 1.0f);
                            return (b - a) * fx + a;
                        }
                    private:
                        enum
                        {
                            // Park, Miller and Stockmeyer (1993)
                            a = 48271,
                            m = 2147483647,
                            rmin = 1,
                            rmax = m-1
                        };

                        // get next random number (linear congruential generator)
                        unsigned generate()
                        {
                            // Schrage's algorithm
                            const unsigned q = m / a;
                            const unsigned r = m % a;
                            const unsigned s = a * (x % q);
                            const unsigned t = r * (x / q);
                            x = s + (s < t) * m - t - (x >= m) * m;
                            return x;
                        }

                        unsigned x=1;
                };

                RNG rng;

                Seconds retryBase;
                Seconds retryMax;
                Seconds requestTimeout;
                int   durableConcurrentRequests;
                Seconds durableDrainInterval;
                Seconds handshakeOffset;
		};

	} /* Core */
} /* GameSparks */

#endif /* GSClientConfig_h__ */
