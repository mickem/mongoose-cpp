#include <string.h>
#include <string>
#include <iostream>
#include <mongoose.h>
#include "Request.h"

using namespace std;

static int lowercase(const char *s) {
  return tolower(* (const unsigned char *) s);
}

static int mg_strncasecmp(const char *s1, const char *s2, size_t len) {
  int diff = 0;

  if (len > 0)
    do {
      diff = lowercase(s1++) - lowercase(s2++);
    } while (diff == 0 && s1[-1] != '\0' && --len > 0);

  return diff;
}

static void mg_strlcpy(register char *dst, register const char *src, size_t n) {
  for (; *src != '\0' && n > 1; n--) {
    *dst++ = *src++;
  }
  *dst = '\0';
}

/*
static int mg_strcasecmp(const char *s1, const char *s2) {
  int diff;

  do {
    diff = lowercase(s1++) - lowercase(s2++);
  } while (diff == 0 && s1[-1] != '\0');

  return diff;
}
*/

static const char *mg_strcasestr(const char *big_str, const char *small_str) {
  int i, big_len = strlen(big_str), small_len = strlen(small_str);

  for (i = 0; i <= big_len - small_len; i++) {
    if (mg_strncasecmp(big_str + i, small_str, small_len) == 0) {
      return big_str + i;
    }
  }

  return NULL;
}

static int mg_get_cookie(const char *cookie_header, const char *var_name,
                  char *dst, size_t dst_size) {
  const char *s, *p, *end;
  int name_len, len = -1;

  if (dst == NULL || dst_size == 0) {
    len = -2;
  } else if (var_name == NULL || (s = cookie_header) == NULL) {
    len = -1;
    dst[0] = '\0';
  } else {
    name_len = (int) strlen(var_name);
    end = s + strlen(s);
    dst[0] = '\0';

    for (; (s = mg_strcasestr(s, var_name)) != NULL; s += name_len) {
      if (s[name_len] == '=') {
        s += name_len + 1;
        if ((p = strchr(s, ' ')) == NULL)
          p = end;
        if (p[-1] == ';')
          p--;
        if (*s == '"' && p[-1] == '"' && p > s + 1) {
          s++;
          p--;
        }
        if ((size_t) (p - s) < dst_size) {
          len = p - s;
          mg_strlcpy(dst, s, (size_t) len + 1);
        } else {
          len = -3;
        }
        break;
      }
    }
  }
  return len;
}

namespace Mongoose
{
    Request::Request(struct mg_connection *connection_, struct http_message *message_) 
		: connection(connection_)
		, message(message_)
    {
        url = std::string(message->uri.p, message->uri.len);
        method = std::string(message->method.p, message->method.len);

        // Downloading POST data
        ostringstream postData;
        postData.write(message->body.p, message->body.len);
        data = postData.str();
    }

    string Request::getUrl()
    {
        return url;
    }

    string Request::getMethod()
    {
        return method;
    }

    string Request::getData()
    {
        return data;
    }

	string Request::getRemoteIp() {
		return std::string(inet_ntoa(connection->sa.sin.sin_addr));
	}

#ifdef ENABLE_REGEX_URL
    smatch Request::getMatches()
    {   
        return matches;
    }   

    bool Request::match(string pattern)
    {   
        key = method + ":" + url;
        return regex_match(key, matches, regex(pattern));
    }   
#endif

    void Request::writeResponse(Response *response)
    {
        string data = response->getData();

		mg_send(connection, data.c_str(), data.size());
    }

    bool Request::hasVariable(string key)
    {
		for (int i = 0 ; i < ARRAY_SIZE(message->header_names); i++) {
			if (strcmp(message->header_names[i].p, key.c_str()) == 0) {
				return true;
			}
		}
        return false;
    }

	Request::arg_vector get_var_vector(const char *data, size_t data_len) {
		Request::arg_vector ret;

		if (data == NULL || data_len == 0)
			return ret;
		
		istringstream f(string(data, data_len));
		string s;    
		char *tmp = new char[data_len+1];
		// data is "var1=val1&var2=val2...". Find variable first
		while (getline(f, s, '&')) {
			string::size_type eq_pos = s.find('=');
			string key, val;
			if (eq_pos != string::npos) {
				key = s.substr(0, eq_pos);
				val = s.substr(eq_pos+1);
			} else {
				key = s;
			}
			if (mg_url_decode(key.c_str(), key.length(), tmp, data_len+1, 1) == -1) {
				delete [] tmp;
				return ret;
			}
			key = tmp;
			if (val.length() > 0) {
				if (mg_url_decode(val.c_str(), val.length(), tmp, data_len+1, 1) == -1) {
					delete [] tmp;
					return ret;
				}
				val = tmp;
			}
			ret.push_back(Request::arg_entry(key, val));
		}
		delete [] tmp;
		return ret;
	}


	Request::arg_vector Request::getVariablesVector() {
		return get_var_vector(message->query_string.p, message->query_string.p == NULL ? 0 : message->query_string.len);
// 		if (len < 0)
// 			len = get_var_vector(conn->content, conn->content_len);
	}

	std::string Request::readHeader(const std::string key) {
		struct mg_str *value = mg_get_http_header(message, key.c_str());
		if (value == NULL || value->len == 0)
			return "";
		return std::string(value->p, value->len);

	}

    bool Request::readVariable(const struct mg_str data, string key, string &output)
    {
        int size = 1024, ret;
        char *buffer = new char[size];

        do {
            ret = mg_get_http_var(&data, key.c_str(), buffer, size);

            if (ret == -1) {
				delete[] buffer;
				return false;
            }

            if (ret == -2) {
                size *= 2;
                delete[] buffer;
                buffer = new char[size];
            }
        } while (ret == -2);

        output = string(buffer);
        delete[] buffer;

        return true;
    }


    string Request::get(string key, string fallback)
    {
        string output;

        // Looking on the query string
        struct mg_str dataField = message->query_string;
        if (readVariable(dataField, key, output)) {
            return output;
        }

        // Looking on the POST data
//         dataField = data.c_str();
//         if (dataField != NULL && readVariable(dataField, key, output)) {
//             return output;
//         }

        return fallback;
    }

    string Request::getCookie(string key, string fallback)
    {
		mg_str *tmp = mg_get_http_header(message, "cookie");
		if (tmp == NULL || tmp->len == 0) {
			return fallback;
		}
		int ret = -1;
		int size = 1024;
		char* buffer = new char[size];
		std::string data = std::string(tmp->p, tmp->len);
		do {
			ret = mg_get_cookie(data.c_str(), key.c_str(), buffer, size);
			if (ret >= 0) {
				std::string ret = buffer;
				delete[] buffer;
				return ret;
			}
			if (ret == -1) {
				delete[] buffer;
				return fallback;
			}
			if (ret == -3) {
				size *= 2;
				delete[] buffer;
				buffer = new char[size];
			}
		} while (ret == -3);
		delete[] buffer;
		return fallback;
    }
            
    void Request::handleUploads()
    {
        char var_name[1024];
        char file_name[1024];
        const char *data;
        size_t data_len;

        if (mg_parse_multipart(message->body.p, message->body.len,
                    var_name, sizeof(var_name), file_name, sizeof(file_name), &data, &data_len)) {
            uploadFiles.push_back(UploadFile(string(file_name), string(data, data_len)));
        }
    }
}
