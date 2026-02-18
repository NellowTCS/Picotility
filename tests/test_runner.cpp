#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <libgen.h>
#include <linux/limits.h>

extern "C" {
#include "pico_vm.h"
#include "pico_ram.h"
#include "pico_cart.h"
}

#define MAX_CARTS 100
#define MAX_FRAMES 100

static char base_path[PATH_MAX];

static void resolve_cart_path(const char* rel_path, char* out_path, size_t out_size) {
    snprintf(out_path, out_size, "%s/%s", base_path, rel_path);
}

typedef struct {
    const char* name;
    const char* path;
    bool skip;
} CartTest;

static CartTest all_carts[] = {
    {"arithmetictest", "carts/arithmetictest.p8", false},
    {"bitwiseandtest", "carts/bitwiseandtest.p8", false},
    {"boldtexttest", "carts/boldtexttest.p8", false},
    {"cartdatatest", "carts/cartdatatest.p8", false},
    {"cartparsetest", "carts/cartparsetest.p8", false},
    {"chr_large_args", "carts/chr_large_args.p8", false},
    {"cliptest", "carts/cliptest.p8", false},
    {"count_val_test", "carts/count_val_test.p8", false},
    {"drillerinputtest", "carts/drillerinputtest.p8", false},
    {"e_next_to_digit", "carts/e_next_to_digit.p8", false},
    {"emojibuttons", "carts/emojibuttons.p8", false},
    {"fillptest", "carts/fillptest.p8", false},
    {"invert_circfill_static", "carts/invert_circfill_static.p8", false},
    {"ld45", "carts/ld45.p8", false},
    {"loop_max_val", "carts/loop_max_val.p8", false},
    {"memorytest", "carts/memorytest.p8", false},
    {"neg_scrn_pal_test", "carts/neg_scrn_pal_test.p8", false},
    {"nested_env_test", "carts/nested_env_test.p8", false},
    {"nilpairstest", "carts/nilpairstest.p8", false},
    {"one_off_chars", "carts/one_off_chars.p8", false},
    {"ord_multiple", "carts/ord_multiple.p8", false},
    {"ord_nil_arg", "carts/ord_nil_arg.p8", false},
    {"p8scii_bg_custom_font_test", "carts/p8scii_bg_custom_font_test.p8", false},
    {"pal_args_test", "carts/pal_args_test.p8", false},
    {"paltabletest", "carts/paltabletest.p8", false},
    {"peek4test", "carts/peek4test.p8", false},
    {"peek_high_addr", "carts/peek_high_addr.p8", false},
    {"peek_large_count", "carts/peek_large_count.p8", false},
    {"peek_poke_extraargs", "carts/peek_poke_extraargs.p8", false},
    {"per_char_width_test", "carts/per_char_width_test.p8", false},
    {"ppwr-big-digit-test", "carts/ppwr-big-digit-test.p8", false},
    {"print_mem_poke", "carts/print_mem_poke.p8", false},
    {"print_scroll_test", "carts/print_scroll_test.p8", false},
    {"pset00-test", "carts/pset00-test.p8", false},
    {"pset3pix", "carts/pset3pix.p8", false},
    {"psetall", "carts/psetall.p8", false},
    {"reloadininit", "carts/reloadininit.p8", false},
    {"return_assign_shortprint_test", "carts/return_assign_shortprint_test.p8", false},
    {"short_print_test", "carts/short_print_test.p8", false},
    {"songtest", "carts/songtest.p8", false},
    {"split_noargs_test", "carts/split_noargs_test.p8", false},
    {"splittest", "carts/splittest.p8", false},
    {"str_index_sub_test", "carts/str_index_sub_test.p8", false},
    {"subtest", "carts/subtest.p8", false},
    {"tablerndtest", "carts/tablerndtest.p8", false},
    {"tilde_bxor_test", "carts/tilde_bxor_test.p8", false},
    {"tline_test", "carts/tline_test.p8", false},
    {"tonumtest2", "carts/tonumtest2.p8", false},
    
    // PNG carts from schoblaska/pico-8
    {"37402", "carts/37402.p8.png", false},
    {"hund3d", "carts/hund3d.p8.png", false},
    {"kaido", "carts/kaido.p8.png", false},
    {"lander", "carts/lander.p8.png", false},
    {"parallax", "carts/parallax.p8.png", false},
    {"raycaster", "carts/raycaster.p8.png", false},
    {"sokotiles_wip1", "carts/sokotiles_wip1.p8.png", false},
    {"sokotiles_wip2", "carts/sokotiles_wip2.p8.png", false},
    {"tvstatic", "carts/tvstatic.p8.png", false},
    {"celeste", "carts/Celeste.p8.png", false},
    {"racer", "carts/racer.p8", false},
};

static int passed = 0;
static int failed = 0;
static int skipped = 0;

bool run_cart_test(const char* name, const char* path, int max_frames) {
    pico_vm_t vm;
    
    if (!pico_vm_init(&vm)) {
        printf("FAIL: %s - failed to init VM: %s\n", name, pico_vm_get_error(&vm));
        return false;
    }
    
    if (!pico_vm_load_cart(&vm, path)) {
        printf("FAIL: %s - failed to load cart: %s\n", name, pico_vm_get_error(&vm));
        pico_vm_shutdown(&vm);
        return false;
    }
    
    pico_vm_run(&vm);
    
    for (int i = 0; i < max_frames && vm.state == PICO_VM_RUNNING; i++) {
        pico_vm_step(&vm);
    }
    
    if (vm.state == PICO_VM_ERROR) {
        printf("FAIL: %s - runtime error at frame %u: %s\n", 
               name, vm.frame_count, pico_vm_get_error(&vm));
        pico_vm_shutdown(&vm);
        return false;
    }
    
    pico_vm_shutdown(&vm);
    return true;
}

static char full_cart_path[PATH_MAX];

const char* get_cart_path(const char* rel_path) {
    resolve_cart_path(rel_path, full_cart_path, sizeof(full_cart_path));
    return full_cart_path;
}

int main(int argc, char** argv) {
    int num_carts = sizeof(all_carts) / sizeof(all_carts[0]);
    int max_frames = MAX_FRAMES;
    
    char exe_path[PATH_MAX];
    char exe_dir[PATH_MAX];
    
    if (argv[0]) {
        realpath(argv[0], exe_path);
        strcpy(exe_dir, dirname(exe_path));
        char* dir_name = basename(exe_dir);
        if (strcmp(dir_name, "build") == 0) {
            char* tests_dir = dirname(exe_dir);
            strcpy(base_path, dirname(tests_dir));
        } else {
            strcpy(base_path, dirname(exe_dir));
        }
    } else {
        getcwd(base_path, sizeof(base_path));
    }
    
    printf("Base path: %s\n", base_path);
    
    if (argc > 1) {
        max_frames = atoi(argv[1]);
    }
    
    printf("Running Picotility Cart Tests\n");
    printf("=============================\n");
    printf("Testing %d carts (max %d frames each)\n\n", num_carts, max_frames);
    
    for (int i = 0; i < num_carts; i++) {
        CartTest* cart = &all_carts[i];
        
        if (cart->skip) {
            printf("SKIP: %s\n", cart->name);
            skipped++;
            continue;
        }
        
        printf("Testing: %s ... ", cart->name);
        fflush(stdout);
        
        if (run_cart_test(cart->name, get_cart_path(cart->path), max_frames)) {
            printf("PASS\n");
            passed++;
        } else {
            failed++;
        }
    }
    
    printf("\n=============================\n");
    printf("Results: %d passed, %d failed, %d skipped\n", passed, failed, skipped);
    
    return failed > 0 ? 1 : 0;
}
