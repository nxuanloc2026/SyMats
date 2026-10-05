// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include "symats_interpreter.hpp"

#include <iostream>

#ifdef SYMATS_HAS_XEUS

namespace symats {

symats_interpreter::symats_interpreter() = default;

void symats_interpreter::configure_impl() {
    // Initialization hook for the kernel session
}

nlohmann::json symats_interpreter::execute_request_impl(
    int execution_counter,
    const std::string& code,
    bool silent,
    bool /*store_history*/,
    nlohmann::json /*user_expressions*/,
    bool /*allow_stdin*/) {

    nlohmann::json reply;
    try {
        std::vector<Statement> stmts = parse_cell(code);
        std::vector<StatementResult> results = session_.run_cell(stmts);

        bool has_error = false;
        std::string err_name, err_val;

        for (const auto& res : results) {
            if (!res.ok()) {
                has_error = true;
                err_name = "EvaluationError";
                err_val = res.error;
                if (!silent) {
                    publish_stream("stderr", res.error + "\n");
                }
                break;
            }

            if (!silent && res.visible()) {
                nlohmann::json pub_data;
                pub_data["text/plain"] = to_text(res.output);
                pub_data["text/latex"] = "$" + to_latex(res.output) + "$";
                nlohmann::json metadata = nlohmann::json::object();
                publish_execution_result(execution_counter, std::move(pub_data), std::move(metadata));
            }
        }

        if (has_error) {
            reply = xeus::create_error_reply(err_name, err_val, {});
        } else {
            reply = xeus::create_successful_reply();
        }
    } catch (const std::exception& ex) {
        if (!silent) {
            publish_stream("stderr", std::string(ex.what()) + "\n");
        }
        reply = xeus::create_error_reply("ParseError", ex.what(), {});
    }

    return reply;
}

nlohmann::json symats_interpreter::complete_request_impl(
    const std::string& /*code*/,
    int /*cursor_pos*/) {
    nlohmann::json result;
    result["matches"] = nlohmann::json::array();
    result["cursor_start"] = 0;
    result["cursor_end"] = 0;
    result["status"] = "ok";
    return result;
}

nlohmann::json symats_interpreter::inspect_request_impl(
    const std::string& /*code*/,
    int /*cursor_pos*/,
    int /*detail_level*/) {
    nlohmann::json result;
    result["found"] = false;
    result["status"] = "ok";
    return result;
}

nlohmann::json symats_interpreter::is_complete_request_impl(
    const std::string& /*code*/) {
    nlohmann::json result;
    result["status"] = "complete";
    return result;
}

nlohmann::json symats_interpreter::kernel_info_request_impl() {
    nlohmann::json result;
    result["implementation"] = "symats";
    result["implementation_version"] = "0.1.0";
    result["language_info"]["name"] = "symats";
    result["language_info"]["version"] = "0.1.0";
    result["language_info"]["mimetype"] = "text/x-symats";
    result["language_info"]["file_extension"] = ".sym";
    result["banner"] = "Symats - Symbolic Mathematics Kernel";
    result["status"] = "ok";
    return result;
}

void symats_interpreter::shutdown_request_impl() {
    // Shutdown hook
}

}  // namespace symats

#endif
