// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include <iostream>

#include "symats/kernel.h"

#if __has_include(<xeus/xinterpreter.hpp>)
#include <xeus/xinterpreter.hpp>
#include <xeus-zmq/xserver_zmq.hpp>
#define SYMATS_HAVE_XEUS 1
#endif

#ifdef SYMATS_HAVE_XEUS

class SymatsXinterpreter : public xeus::xinterpreter {
public:
    SymatsXinterpreter() = default;
    ~SymatsXinterpreter() override = default;

protected:
    void configure_impl() override {}

    nl::json execute_request_impl(int execution_counter,
                                  const std::string& code,
                                  bool /*silent*/,
                                  bool /*store_history*/,
                                  nl::json /*user_expressions*/,
                                  bool /*allow_stdin*/) override {
        auto results = engine_.execute_cell(code);

        nl::json pub_data;
        std::string plain_combined;
        std::string latex_combined;
        bool has_error = false;
        std::string error_msg;

        for (const auto& res : results) {
            if (!res.ok) {
                has_error = true;
                error_msg = res.error_message;
            } else if (!res.text_output.empty()) {
                if (!plain_combined.empty()) plain_combined += "\n";
                plain_combined += "Out[" + std::to_string(res.line_number) + "] = " + res.text_output;

                if (!latex_combined.empty()) latex_combined += "\n";
                latex_combined += res.latex_output;
            }
        }

        if (!plain_combined.empty()) {
            pub_data["text/plain"] = plain_combined;
            if (!latex_combined.empty()) {
                pub_data["text/latex"] = latex_combined;
            }
            publish_execution_result(execution_counter, std::move(pub_data), nl::json::object());
        }

        if (has_error) {
            publish_execution_error("EvaluationError", error_msg, {error_msg});
            return xeus::create_error_reply("EvaluationError", error_msg, {error_msg});
        }

        return xeus::create_successful_reply();
    }

    nl::json complete_request_impl(const std::string& /*code*/,
                                   int /*cursor_pos*/) override {
        return xeus::create_complete_reply({}, 0, 0);
    }

    nl::json inspect_request_impl(const std::string& /*code*/,
                                  int /*cursor_pos*/,
                                  int /*detail_level*/) override {
        return xeus::create_inspect_reply(false, {}, {});
    }

    nl::json is_complete_request_impl(const std::string& /*code*/) override {
        return xeus::create_is_complete_reply("complete");
    }

    void kernel_info_request_impl(nl::json& result) override {
        result["implementation"] = "symats";
        result["implementation_version"] = "0.1.0";
        result["language_info"]["name"] = "symats";
        result["language_info"]["version"] = "0.1.0";
        result["language_info"]["mimetype"] = "text/x-symats";
        result["language_info"]["file_extension"] = ".sym";
    }

    void shutdown_request_impl() override {}

private:
    symats::SymatsKernelEngine engine_;
};

int main(int argc, char* argv[]) {
    std::string connection_filename = (argc > 1) ? argv[1] : "connection.json";
    xeus::xconfiguration config = xeus::load_configuration(connection_filename);
    xeus::xkernel kernel(config, xeus::get_user_name(), xeus::make_xserver_zmq);

    SymatsXinterpreter interpreter;
    kernel.start(interpreter);
    return 0;
}

#else

int main() {
    std::cout << "Symats Jupyter Kernel engine interface\n";
    symats::SymatsKernelEngine engine;
    auto res = engine.execute_cell("x = 5;\nSin[x] + 1");
    for (const auto& r : res) {
        if (r.ok && !r.text_output.empty()) {
            std::cout << "Out[" << r.line_number << "] = " << r.text_output << "\n";
        } else if (!r.ok) {
            std::cout << "Error: " << r.error_message << "\n";
        }
    }
    return 0;
}

#endif
