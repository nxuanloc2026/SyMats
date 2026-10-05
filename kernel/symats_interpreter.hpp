// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#pragma once

#include <memory>
#include <string>

#include "symats/latex.h"
#include "symats/session.h"
#include "symats/text.h"

#ifdef SYMATS_HAS_XEUS
#include <nlohmann/json.hpp>
#include <xeus/xinterpreter.hpp>

namespace symats {

class symats_interpreter : public xeus::xinterpreter {
public:
    symats_interpreter();
    ~symats_interpreter() override = default;

protected:
    void configure_impl() override;

    nlohmann::json execute_request_impl(
        int execution_counter,
        const std::string& code,
        bool silent,
        bool store_history,
        nlohmann::json user_expressions,
        bool allow_stdin) override;

    nlohmann::json complete_request_impl(
        const std::string& code,
        int cursor_pos) override;

    nlohmann::json inspect_request_impl(
        const std::string& code,
        int cursor_pos,
        int detail_level) override;

    nlohmann::json is_complete_request_impl(
        const std::string& code) override;

    nlohmann::json kernel_info_request_impl() override;

    void shutdown_request_impl() override;

private:
    Session session_;
};

}  // namespace symats
#else
namespace symats {

class symats_interpreter {
public:
    symats_interpreter() = default;
    Session& session() { return session_; }

private:
    Session session_;
};

}  // namespace symats
#endif
