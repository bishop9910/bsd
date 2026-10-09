#include <iostream>
#include "CLI/CLI.hpp"
#include "core.hpp"

int main(int argc, char **argv) {
    CLI::App app{"App description"};
    CLI11_PARSE(app, argc, argv);
    BSD_CORE::initDownloadCore();
    return 0;
}