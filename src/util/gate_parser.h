#ifndef GATE_PARSER_H
#define GATE_PARSER_H

#include <stdio.h>

#include <string>
#include <list>

namespace Gate {

class Parser {
  class CommandChain {
    CommandChain() : is_valid_(true) {}

   public:
    using string_list = std::list<std::string>;
    string_list::const_iterator commands() const { return commands_.begin(); }
    string_list::const_iterator arguments() const { return arguments_.begin(); }
    bool is_valid() const { return is_valid_; }

    friend class Parser;

   private:
    bool is_valid_;
    std::list<std::string> commands_;
    std::list<std::string> arguments_;
  };

  std::string::size_type parse_command(std::string input, CommandChain& chain) {
    auto start = input.find_first_of("/", 0);

    if (start == std::string::npos) {
      fprintf(stderr, "`%s' is not a gate command ", input.c_str());
      chain.is_valid_ = false;
      return std::string::npos;
    }

    start++;
    bool another_command = true;
    while (1) {
      auto next_backslash = input.find_first_of("/ \t\n", start);
      if (next_backslash == std::string::npos) {
        fprintf(stderr, "`%s' is not a gate command ", input.c_str());
        chain.is_valid_ = false;
        another_command = false;
        return std::string::npos;
      }
      auto command = input.substr(start, next_backslash - start);
      chain.commands_.push_back(command);
      if (input[next_backslash] == '/')
        start = next_backslash + 1;
      else
        break;
    }
    return start;
  }

  std::string::size_type parse_arguments(std::string input,
                                         CommandChain& chain) {
    return std::string::npos;
  }

 public:
  using CommandChain = Parser::CommandChain;

  CommandChain parse(std::string line) {
    CommandChain chain;

    auto arguments_start = parse_command(line, chain);

    if (arguments_start == std::string::npos) {
      fprintf(stderr, "`%s' is not a gate command ", line.c_str());
      return chain;
    }

    while (!std::isalpha(line[arguments_start]) &&
           arguments_start < line.size())
      arguments_start++;

    if (arguments_start == line.size())
      return chain;

    parse_arguments(line.substr(arguments_start, line.size() - arguments_start),
                    chain);
  }
};
}

#endif  // GATE_PARSER_H
