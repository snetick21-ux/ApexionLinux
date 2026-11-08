// apx — пакетный менеджер Apexion Linux.
//
// Это фронтенд над apt/dpkg: короткие команды, проверка аргументов,
// запуск программ напрямую (без shell) и права root через sudo.
// Сборка без CMake:  g++ -std=c++17 -O2 -Wall -Wextra -o apx src/main.cpp

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

namespace {

constexpr const char* kVersion = "0.1.0";

int fail(const std::string& msg) {
    std::cerr << "apx: " << msg << '\n';
    return 2;
}

void usage() {
    std::cout << R"APX(apx — пакетный менеджер Apexion Linux

Использование:
  apx update                 обновить список пакетов
  apx upgrade                обновить установленные пакеты
  apx install <имя>...       установить пакеты
  apx install <файл.deb>     установить локальный .deb
  apx remove <имя>...        удалить пакеты (конфиги оставить)
  apx purge <имя>...         удалить пакеты вместе с конфигами
  apx autoremove             убрать ненужные зависимости
  apx search <слово>...      искать пакеты
  apx info <имя>             описание пакета
  apx list                   установленные пакеты (ii = установлен)
  apx version                версия

Команды, которые меняют систему, сами запросят sudo.
)APX";
}

// Запускает программу напрямую, без shell: аргументы не разбираются,
// поэтому через имя пакета нельзя внедрить лишнюю команду.
int run(const std::vector<std::string>& args) {
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);

    const pid_t pid = fork();
    if (pid < 0) {
        std::perror("fork");
        return 1;
    }
    if (pid == 0) {
        execvp(argv[0], argv.data());
        std::perror(argv[0]);
        _exit(127);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        std::perror("waitpid");
        return 1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

// Команды, меняющие систему, нужны root: перезапускаем apx через sudo.
[[noreturn]] void restartWithSudo(int argc, char** argv) {
    std::cerr << "apx: нужны права администратора, запускаю через sudo...\n";
    std::vector<char*> nargv{const_cast<char*>("sudo"), const_cast<char*>("--")};
    for (int i = 0; i < argc; ++i) nargv.push_back(argv[i]);
    nargv.push_back(nullptr);
    execvp("sudo", nargv.data());
    std::perror("sudo");
    std::exit(1);
}

// Имя пакета Debian: строчные буквы, цифры, + . -, при желании суффикс :архитектура.
bool isValidPackageName(const std::string& name) {
    static const std::regex re(R"(^[a-z0-9][a-z0-9+.\-]+(:[a-z0-9\-]+)?$)");
    return std::regex_match(name, re);
}

bool isValidSearchTerm(const std::string& s) {
    static const std::regex re(R"(^[A-Za-z0-9][A-Za-z0-9+._\-]*$)");
    return std::regex_match(s, re);
}

bool isDebFile(const std::string& s) {
    return s.size() > 4 && s.compare(s.size() - 4, 4, ".deb") == 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        usage();
        return 0;
    }

    const std::string cmd = argv[1];
    const std::vector<std::string> args(argv + 2, argv + argc);

    if (cmd == "-h" || cmd == "--help" || cmd == "help") {
        usage();
        return 0;
    }
    if (cmd == "version" || cmd == "-v" || cmd == "--version") {
        std::cout << "apx " << kVersion << '\n';
        return 0;
    }

    // Сначала проверяем аргументы и собираем команду, и только потом поднимаем права.
    std::vector<std::string> plan;
    bool needRoot = false;

    if (cmd == "update") {
        if (!args.empty()) return fail("команда 'update' не принимает аргументов");
        plan = {"apt-get", "update"};
        needRoot = true;
    } else if (cmd == "upgrade" || cmd == "autoremove") {
        if (!args.empty()) return fail("команда '" + cmd + "' не принимает аргументов");
        plan = {"apt-get", "-y", cmd};
        needRoot = true;
    } else if (cmd == "install" || cmd == "remove" || cmd == "purge") {
        if (args.empty()) {
            return fail(cmd == "install" ? "укажи пакет или .deb-файл" : "укажи пакет");
        }
        plan = {"apt-get", "-y", cmd};
        for (const auto& a : args) {
            if (cmd == "install" && isDebFile(a)) {
                if (access(a.c_str(), R_OK) != 0) return fail("не могу прочитать файл: " + a);
                // apt берёт локальный .deb по пути; "./" не даёт имени сойти за имя пакета
                plan.push_back(a.find('/') == std::string::npos ? "./" + a : a);
            } else if (isValidPackageName(a)) {
                plan.push_back(a);
            } else {
                return fail("некорректное имя пакета: " + a);
            }
        }
        needRoot = true;
    } else if (cmd == "search") {
        if (args.empty()) return fail("укажи слово для поиска");
        for (const auto& a : args) {
            if (!isValidSearchTerm(a)) return fail("некорректный запрос: " + a);
        }
        plan = {"apt-cache", "search"};
        plan.insert(plan.end(), args.begin(), args.end());
    } else if (cmd == "info") {
        if (args.size() != 1 || !isValidPackageName(args[0])) {
            return fail("использование: apx info <имя>");
        }
        plan = {"apt-cache", "show", args[0]};
    } else if (cmd == "list") {
        plan = {"dpkg-query", "-W", "-f", "${db:Status-Abbrev} ${Package} ${Version}\\n"};
    } else {
        std::cerr << "apx: неизвестная команда: " << cmd << "\n\n";
        usage();
        return 2;
    }

    if (needRoot && geteuid() != 0) restartWithSudo(argc, argv);

    // dpkg не должен задавать вопросы: везде берём ответы по умолчанию
    setenv("DEBIAN_FRONTEND", "noninteractive", 1);
    return run(plan);
}
