#include "sms.h"
#include "common.h"
#include <curl/curl.h>

bool endworker = false;

std::mutex lock;
std::vector<_SMS_INFO>vsmsinfo;

void smsworker()
{
	std::vector<_SMS_INFO>vbuffer;
	std::vector<_SMS_INFO>::iterator iter;
	char sbuf[100] = { 0 };

	CURL* hnd = curl_easy_init();

	while (true) {

		lock.lock();
		vsmsinfo.swap(vbuffer);
		lock.unlock();

		for (iter = vbuffer.begin(); iter != vbuffer.end(); iter++) {
			_SMS_INFO smsinfo = *iter;
			curl_easy_setopt(hnd, CURLOPT_URL, "http://muengine.org/smsapi.php");
			sprintf(sbuf, "smsnumber=%s&otpmsg=%s", smsinfo.mobilenumber.c_str(), smsinfo.otpmsg.c_str());
			curl_easy_setopt(hnd, CURLOPT_POSTFIELDS, sbuf);
			CURLcode ret = curl_easy_perform(hnd);
			if (ret != CURLE_OK) {
				msglog(DEBUG, "smsworker, curl_easy_perform error code %d, mobile number %s.", ret, smsinfo.mobilenumber.c_str());
				continue;
			}
			msglog(DEBUG, "smsworker, sent otp code to mobile number %s.", smsinfo.mobilenumber.c_str());
		}

		vbuffer.clear();
		if (endworker)
			break;
		sleep(100);
	}

	curl_easy_cleanup(hnd);
	curl_global_cleanup();
}
