#include <filesystem>
#include <thread>
#include <magic_enum.hpp>
#include <spdlog/spdlog.h>

#include "http.hpp"
#include "../client/domain_resolver.hpp"
#include "../utils/network.hpp"

namespace server {
Http::Http(core::Core* core)
    : core_{ core }
    , server_{ "./resources/cert.pem", "./resources/key.pem" }
{

}

Http::~Http()
{
    stop();
}

bool Http::bind_to_port(const std::string& host, int port)
{
    if (server_.bind_to_port(host, port)) {
        spdlog::info("HTTP(s) server listening on port {}.", port);
        return true;
    }

    spdlog::error("Failed to bind to port {}.", port);
    return false;
}

void Http::listen_after_bind()
{
    std::thread{ &Http::listen_internal, this }.detach();
}

bool Http::listen(const std::string& host, int port)
{
    if (!bind_to_port(host, port)) {
        return false;
    }

    listen_after_bind();
    return true;
}

void Http::stop()
{
    server_.stop();
}

bool validate_server_response(const httplib::Result& response)
{
    if (!response) {
        spdlog::error(
            "Response is null with error: httplib::Error::{}",
            magic_enum::enum_name(response.error())
        );
        return false;
    }

    const httplib::Error error_response{ response.error() };
    const int status_code{ response->status };

    if (error_response != httplib::Error::Success || status_code != 200) {
        spdlog::error(
            "Failed to get server data. {}.",
            error_response == httplib::Error::Success
                ? fmt::format("HTTP status code: {}", status_code)
                : fmt::format("HTTP error: {}", httplib::to_string(error_response))
        );
        return false;
    }

    spdlog::debug("Got server data. HTTP status code: {}", status_code);
    return true;
}

std::string resolve_ip_address(std::string_view host)
{
    std::string resolved_ip{ host };
    if (network::classify_host(resolved_ip) == network::HostType::Hostname) {
        auto [status, ip]{ domain_resolver::resolve_domain_name(resolved_ip) };

        if (status != domain_resolver::DomainResolverStatus::NoError) {
            spdlog::error(
                "Error occurred while resolving {} ip address. Dns server returned {}",
                host,
                magic_enum::enum_name(status)
            );
            return {};
        }

        resolved_ip = ip;
        spdlog::info("{} ip address is {}", host, resolved_ip);
    }

    return resolved_ip;
}

std::string send_request_to_server(std::string_view resolved_ip, const httplib::Headers& headers, const httplib::Params& params)
{
    httplib::Client cli{ fmt::format("https://{}", resolved_ip) };
    cli.enable_server_certificate_verification(false);

    const httplib::Headers header{
        { "User-Agent", get_header_value(headers, "User-Agent") },
        { "Host", get_header_value(headers, "Host") }
    };

    httplib::Result response{ cli.Post("/growtopia/server_data.php", header, params) };
    if (validate_server_response(response)) {
        if (!response->body.empty()) {
            return response->body;
        }
    }

    // Not sure if this is needed. New Growtopia client doesn't send a GET request to server_data.php
    response = cli.Get("/growtopia/server_data.php");
    if (validate_server_response(response)) {
        if (!response->body.empty()) {
            return response->body;
        }
    }

    spdlog::warn("Failed to retrieve server data from {}", resolved_ip);
    return {};
}

std::string get_server_data(std::string_view host, const httplib::Headers& headers, const httplib::Params& params)
{
    spdlog::debug("Requesting server data from: https://{}", host);

    const std::string resolved_ip{ resolve_ip_address(host) };
    if (resolved_ip.empty()) {
        return {};
    }

    return send_request_to_server(resolved_ip, headers, params);
}

void Http::listen_internal()
{
    server_.set_logger([](const httplib::Request& req, const httplib::Response& res) {
        spdlog::info("{} {} {}", req.method, req.path, res.status);
    });

    server_.set_error_handler([](const httplib::Request& req, httplib::Response& res) {
        res.set_content(
            fmt::format(
                "Hello, world!\r\n{} ({})",
                httplib::detail::status_message(res.status),
                res.status
            ),
            "text/plain"
        );
    });

    server_.set_exception_handler([](
        const httplib::Request& req,
        httplib::Response& res,
        const std::exception_ptr& ep
    ) {
        res.status = 500;

        try {
            std::rethrow_exception(ep);
        }
        catch (std::exception &e) {
            res.set_content(fmt::format("Hello, world!\r\n{}", e.what()), "text/plain");
        }
        catch (...) {
            res.set_content("Hello, world!\r\nUnknown Exception", "text/plain");
        }
    });

    server_.Post("/growtopia/server_data.php", [&](
        const httplib::Request& req,
        httplib::Response& res
    ) {
        if (!req.headers.empty()) {
            spdlog::info("Headers:");
            for (auto& header : req.headers) {
                spdlog::info("\t{}: {}", header.first, header.second);
            }
        }

        if (!req.params.empty()) {
            spdlog::info("Params:");
            spdlog::info("\t{}", httplib::detail::params_to_query_str(req.params));
        }

        TextParse text_parse{ get_server_data(core_, req.headers, req.params) };

        spdlog::debug("Sending server data: {}", text_parse.get_raw());

        text_parse.set("server", { "127.0.0.1" });
        text_parse.set("port", { std::to_string(core_->get_config().get<int>("server.port")) });
        text_parse.set("type2", { "1" });

        spdlog::debug("Sending server data: {}", text_parse.get_raw());

        res.set_content(text_parse.get_raw(), "text/html");
        return true;
    });

    server_.listen_after_bind();
}
}
