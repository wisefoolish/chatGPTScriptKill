#include "authorization.h"

bool liboai::Authorization::SetKey(std::string_view key) noexcept {
	if (!key.empty()) {
		this->key_ = key;
		if (this->auth_headers_.count("Authorization") > 0) {
			this->auth_headers_.erase("Authorization");
		}
		this->auth_headers_["Authorization"] = ("Bearer " + this->key_);
		return true;
	}
	return false;
}

bool liboai::Authorization::SetKeyFile(const std::filesystem::path& path) noexcept {
	if (std::filesystem::exists(path) && std::filesystem::is_regular_file(path) && std::filesystem::file_size(path) > 0) {
		std::ifstream file(path);
		if (file.is_open()) {
			std::getline(file, this->key_);
			if (this->auth_headers_.count("Authorization") > 0) {
				this->auth_headers_.erase("Authorization");
			}
			this->auth_headers_["Authorization"] = ("Bearer " + this->key_);
			return true;
		}
	}
	return false;
}

bool liboai::Authorization::SetKeyEnv(std::string_view var) noexcept {
	if (!var.empty()) {
		const char* key = var.data();
		if (key != nullptr) {
			this->key_ = key;
			if (this->auth_headers_.count("Authorization") > 0) {
				this->auth_headers_.erase("Authorization");
			}
			this->auth_headers_["Authorization"] = ("Bearer " + this->key_);
			return true;
		}
		return false;
	}
	return false;
}

bool liboai::Authorization::SetOrganization(std::string_view org) noexcept {
	if (!org.empty()) {
		this->org_ = std::move(org);
		if (this->auth_headers_.count("OpenAI-Organization") > 0) {
			this->auth_headers_.erase("OpenAI-Organization");
		}
		this->auth_headers_["OpenAI-Organization"] = this->org_;
		return true;
	}
	return false;
}

bool liboai::Authorization::SetOrganizationFile(const std::filesystem::path& path) noexcept {
	if (std::filesystem::exists(path) && std::filesystem::is_regular_file(path) && std::filesystem::file_size(path) > 0) {
		std::ifstream file(path);
		if (file.is_open()) {
			std::getline(file, this->key_);
			if (this->auth_headers_.count("OpenAI-Organization") > 0) {
				this->auth_headers_.erase("OpenAI-Organization");
			}
			this->auth_headers_["OpenAI-Organization"] = this->org_;
			return true;
		}
	}
	return false;
}

bool liboai::Authorization::SetOrganizationEnv(std::string_view var) noexcept {
	if (!var.empty()) {
		const char* org = std::getenv(var.data());
		if (org != nullptr) {
			this->org_ = org;
			if (this->auth_headers_.count("OpenAI-Organization") > 0) {
				this->auth_headers_.erase("OpenAI-Organization");
			}
			this->auth_headers_["OpenAI-Organization"] = this->org_;
			return true;
		}
		return false;
	}
	return false;
}

void liboai::Authorization::SetProxies(const std::initializer_list<std::pair<const std::string, std::string>>& hosts) noexcept {
	this->proxies_ = netimpl::components::Proxies(hosts);
}

void liboai::Authorization::SetProxies(std::initializer_list<std::pair<const std::string, std::string>>&& hosts) noexcept {
	this->proxies_ = netimpl::components::Proxies(std::move(hosts));
}

void liboai::Authorization::SetProxies(const std::map<std::string, std::string>& hosts) noexcept {
	this->proxies_ = netimpl::components::Proxies(hosts);
}

void liboai::Authorization::SetProxies(std::map<std::string, std::string>&& hosts) noexcept {
	this->proxies_ = netimpl::components::Proxies(std::move(hosts));
}

void liboai::Authorization::SetProxyAuth(const std::map<std::string, netimpl::components::EncodedAuthentication>& proto_up) noexcept {
	this->proxyAuth_ = netimpl::components::ProxyAuthentication(proto_up);
}