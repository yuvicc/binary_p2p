#include "binary_p2p.h"
#include "message_header.h"
#include <iostream>

int main() {
    MessageHeader ping{"ping"};
    std::cout << ping.command_name();
}
