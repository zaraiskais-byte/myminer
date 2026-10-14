#pragma once
#include <httplib.h>
#include <string>

void register_google_auth_routes(httplib::Server &svr,
                                  const std::string &client_id,
                                  const std::string &client_secret,
                                  const std::string &redirect_uri);
