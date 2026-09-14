#include "parser/parser.h"
#include "typecheck/typecheck.h"
#include "backend_vm/vm.h"
#include "backend_c/transpiler.h"
#include "runtime/http/http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>

static char *read_file_contents(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc(len + 1);
    size_t r = fread(buf, 1, len, f);
    (void)r;
    buf[len] = '\0';
    fclose(f);
    return buf;
}

static time_t get_file_mtime(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return st.st_mtime;
    }
    return 0;
}

static void print_usage(void) {
    printf("Jaguar Compiler Toolchain v1.0.0\n");
    printf("Usage:\n");
    printf("  jag <file.jag>                 Run file via interpreter\n");
    printf("  jag run <file.jag>             Explicit interpreter run\n");
    printf("  jag -live=1 <file.jag>         Run file in live reload mode\n");
    printf("  jag build <file.jag> [-o out]  Build native executable\n");
    printf("  jag check <file.jag>           Parse and typecheck file only\n");
    printf("  jag --version                  Print version\n");
    printf("  jag --help                     Print help\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    if (strcmp(argv[1], "--version") == 0) {
        printf("jag version 1.0.0\n");
        return 0;
    }

    if (strcmp(argv[1], "--help") == 0) {
        print_usage();
        return 0;
    }

    bool live_mode = false;
    const char *cmd = NULL;
    const char *filepath = NULL;
    const char *output_path = "a.out";

    int arg_i = 1;
    if (strncmp(argv[arg_i], "-live=", 6) == 0) {
        live_mode = (strcmp(argv[arg_i] + 6, "1") == 0);
        arg_i++;
        if (arg_i < argc) filepath = argv[arg_i];
    } else if (strcmp(argv[arg_i], "run") == 0) {
        cmd = "run";
        arg_i++;
        if (arg_i < argc) filepath = argv[arg_i];
    } else if (strcmp(argv[arg_i], "check") == 0) {
        cmd = "check";
        arg_i++;
        if (arg_i < argc) filepath = argv[arg_i];
    } else if (strcmp(argv[arg_i], "build") == 0) {
        cmd = "build";
        arg_i++;
        if (arg_i < argc) filepath = argv[arg_i];
        if (arg_i + 2 < argc && strcmp(argv[arg_i + 1], "-o") == 0) {
            output_path = argv[arg_i + 2];
        }
    } else {
        filepath = argv[arg_i];
    }

    if (!filepath) {
        fprintf(stderr, "Error: No input file specified.\n");
        return 1;
    }

    if (cmd && strcmp(cmd, "check") == 0) {
        char *code = read_file_contents(filepath);
        if (!code) {
            fprintf(stderr, "Error: Cannot open file '%s'\n", filepath);
            return 1;
        }
        Parser p;
        parser_init(&p, code);
        ASTNode *ast = parse_program(&p);
        if (p.had_error) {
            fprintf(stderr, "Check failed with syntax errors.\n");
            free(code);
            return 1;
        }
        TypeChecker tc;
        typechecker_init(&tc, live_mode);
        bool ok = typecheck_ast(&tc, ast);
        if (!ok) {
            fprintf(stderr, "Check failed with type errors.\n");
            free(code);
            return 1;
        }
        printf("Check passed successfully!\n");
        free(code);
        ast_node_free(ast);
        typechecker_cleanup(&tc);
        return 0;
    }

    if (cmd && strcmp(cmd, "build") == 0) {
        char *code = read_file_contents(filepath);
        if (!code) {
            fprintf(stderr, "Error: Cannot open file '%s'\n", filepath);
            return 1;
        }
        Parser p;
        parser_init(&p, code);
        ASTNode *ast = parse_program(&p);

        TypeChecker tc;
        typechecker_init(&tc, live_mode);
        if (!typecheck_ast(&tc, ast)) {
            fprintf(stderr, "Build failed due to type errors.\n");
            return 1;
        }

        char gen_c_file[256];
        snprintf(gen_c_file, sizeof(gen_c_file), "/tmp/jag_gen_%d.c", getpid());
        FILE *f = fopen(gen_c_file, "w");
        transpile_ast_to_c(ast, f);
        fclose(f);

        char sys_cmd[1024];
        snprintf(sys_cmd, sizeof(sys_cmd), "gcc -Wall -Wextra -std=c11 -D_GNU_SOURCE -I. runtime/core/value.c runtime/json/json.c runtime/async/async.c runtime/http/http.c runtime/ws/ws.c runtime/worker/worker.c %s -o %s -lpthread -lssl -lcrypto -lm", gen_c_file, output_path);
        int res = system(sys_cmd);
        unlink(gen_c_file);

        if (res == 0) {
            printf("Successfully built native executable: %s\n", output_path);
            return 0;
        } else {
            fprintf(stderr, "Build compilation failed.\n");
            return 1;
        }
    }

    time_t last_mtime = 0;
    for (;;) {
        time_t mtime = get_file_mtime(filepath);
        if (mtime > last_mtime) {
            last_mtime = mtime;
            jag_server_close();

            char *code = read_file_contents(filepath);
            if (!code) {
                fprintf(stderr, "Error reading source file '%s'\n", filepath);
                if (!live_mode) break;
                sleep(1);
                continue;
            }

            Parser p;
            parser_init(&p, code);
            ASTNode *ast = parse_program(&p);

            TypeChecker tc;
            typechecker_init(&tc, live_mode);
            if (typecheck_ast(&tc, ast)) {
                VM vm;
                vm_init(&vm, live_mode);
                vm_eval_ast(&vm, ast);
            }

            ast_node_free(ast);
            typechecker_cleanup(&tc);
            free(code);
        }

        if (!live_mode) break;
        usleep(500000);
    }

    return 0;
}
