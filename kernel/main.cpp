// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 Loc Ngo and Symats contributors
#include <iostream>
#include <memory>
#include <string>

#include "symats_interpreter.hpp"

#ifdef SYMATS_HAS_XEUS
#include <xeus-zmq/xserver_zmq.hpp>
#include <xeus/xkernel.hpp>

int main(int argc, char* argv[]) {
    std::string connection_file;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-f" && i + 1 < argc) {
            connection_file = argv[++i];
        } else if (arg.rfind("-f=", 0) == 0) {
            connection_file = arg.substr(3);
        }
    }

    auto context = xeus::make_context<zmq::context_t>();
    auto interpreter = std::make_unique<symats::symats_interpreter>();
    xeus::xconfiguration config = connection_file.empty()
        ? xeus::xconfiguration{}
        : xeus::load_configuration(connection_file);

    xeus::xkernel kernel(config, xeus::get_user_name(), std::move(context),
                         std::move(interpreter), xeus::make_xserver_zmq);
    std::cout << "Starting Symats Jupyter Kernel..." << std::endl;
    kernel.start();
    return 0;
}
#else
int main() {
    std::cout << "Symats Jupyter Kernel (requires xeus)" << std::endl;
    return 0;
}
#endif
