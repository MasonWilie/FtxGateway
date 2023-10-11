#include <iostream>

#include <Gateway.h>

static void PrintArgsHelp()
{
    std::cout << "Program options format ---" << std::endl
        << "./executable \"<api_key>\" \"<api_secret>\" \"<market>\"" << std::endl;
}

static void RunConsole(ftx::Gateway& gateway)
{
    while (true)
    {
        std::cout << "Buy (b), Sell (s), or Quit (q) > ";
        
        std::string command;

        std::getline(std::cin, command);

        if (command == "q")
        {
            break;
        }
        else if (command == "b" || command == "s")
        {
            const ftx::ws::Side side = command == "b" ? ftx::ws::Side::BUY : ftx::ws::Side::SELL;
            
            std::cout << "Size > ";
            std::getline(std::cin, command);

            const double size = std::stod(command);

            gateway.SendMarketOrder(side, size);

            std::cout << "Sent order..." << std::endl;
        }
        else
        {
            std::cout << std::endl << "Invalid command... " << std::endl;
        }
    }

    std::cout << "Exiting" << std::endl;
}

int main(int argc, char** argv)
{
    if (argc < 4)
    {
        std::cout << "Invalid program arguments" << std::endl;
        PrintArgsHelp();
    }

    const std::string key = argv[1];
    const std::string secret = argv[2];
    const std::string market = argv[3];

    ftx::Gateway gateway(key, secret, market);

    RunConsole(gateway);

    return 0;
}