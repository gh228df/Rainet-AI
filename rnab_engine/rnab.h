// #undef RNAB_DEBUG

#include "librnab.c"

#ifdef RNAB_BENCHMARK

typedef struct
{
    generic_representation position;
    int32_t depth;
} benchmark_t;

static const benchmark_t benchmark_array[] = {
    //  {(generic_representation){4503599627370498ULL,576460752303423589ULL,4503599627370597ULL,1,59,52,61,1,3,2,3,1,1}, 30},
    // {(generic_representation){0x0000000840040000, 0x000000000010001a, 0x0000000840100018,30,20,-1,-1,0,3,3,3,1,0}, 18},
    // {(generic_representation){0x0000000000080090, 0x0002000002000008, 0x0002000002080080,19,49,26,10,2,3,2,3,1,0}, 18},
    // {(generic_representation){0x00000000000100a0, 0x0000000000080600, 0x0000000000010680,16,19,15,-1,2,3,2,3,1,0}, 16},
    // {(generic_representation){0x0410048000000000, 0x08000000000000e5, 0x04000400000000a5,58,59,42,-1,0,2,2,2,1,1}, 14},
    // {(generic_representation){16652059622202408960ULL,6375ULL,2742692173068632229ULL,-1,11,-1,-1,0,0,0,0,1,1}, 14},

    // {(generic_representation){2251800887427072ULL,563018672898048ULL,2814749767106560ULL,51,49,51,50,3,3,3,3,0,0}, 26},
    // {(generic_representation){0x06a8000000001000, 0x10000000000000c3, 0x1020000000001001, 12, 60, 53, 0, 2, 1, 2, 0, 0, 0}, 12},
    // {(generic_representation){0x06a8000000000020, 0x40000000000000c3, 0x0220000000000081, 5, 62, 53, 0, 2, 1, 2, 0, 1, 1}, 16},

    // {(generic_representation){1152921504606847105ULL,2251799813685346ULL,2251799813685441ULL,0,5,7,51,2,2,2,3,1,0}, 16},
    {(generic_representation){0xe718000000000000, 0x00000000000018e7, 0xa5000000000000a5, -1, -1, -1, -1, 0, 0, 0, 0, 1, 1}, 16},
    // {(generic_representation){0x8008000010000000, 0x2000000000003007, 0x0008000010000005, 28, 61, 51, -1, 2, 0, 2, 3, 0, 1}, 22},
    // {(generic_representation){0x8008001000000000, 0x0040000000001027, 0x8008000000000005, 36, 54, 51, -1, 2, 0, 2, 3, 1, 1}, 14},
    // {(generic_representation){0x0004000000000420, 0x0400020001000010, 0x0004000001000400, 10, 4, 50, 61, 2, 1, 3, 3, 0, 0}, 18},
    // {(generic_representation){0x1000000800000000, 0x0800040001000000, 0x0000040801000000, -1, 42, 35, 52, 2, 3, 3, 3, 1, 1}, 20},
    // {(generic_representation){0x4008000010000000, 0x2000000000003007, 0x0008000010000005, 28, 61, 51, 61, 2, 0, 2, 3, 0, 1}, 14},
    // {(generic_representation){0xa100000000100000, 0x00000000000000a8, 0xa000000000000020, 20, 5, -1, 5, 3, 1, 2, 3, 1, 1}, 16},
    {(generic_representation){0x8101000000000100, 0x0000004000000004, 0x8000000000000104, 8, 38, 63, 2, 2, 3, 3, 2, 0, 0}, 26},
};

#define CHECK_WIN()                          \
    if (pos.args.fields.sec_link == 4)       \
    {                                        \
        _printf("Player one wins!\n");       \
        print_field(&pos);                   \
        break;                               \
    }                                        \
    else if (pos.args.fields.sec_virus == 4) \
    {                                        \
        _printf("Player one loses!\n");      \
        print_field(&pos);                   \
        break;                               \
    }                                        \
    else if (pos.args.fields.fir_link == 4)  \
    {                                        \
        _printf("Player two wins!\n");       \
        print_field(&pos);                   \
        break;                               \
    }                                        \
    else if (pos.args.fields.fir_virus == 4) \
    {                                        \
        _printf("Player one loses!\n");      \
        print_field(&pos);                   \
        break;                               \
    }

static void simulate_game(generic_representation *__restrict__ init_game_state, bool player_to_move, int32_t analysis_depth)
{
    field_t pos;
    generic_representation temp_rep;
    minimax_main_result_t move;

    generic_representation_to_intern(init_game_state, &pos);
    print_field(&pos);

    for (;;)
    {
        TIME_TYPE start_it, stop_it;

        _printf("pos: ");
        intern_to_generic_representation(&pos, &temp_rep);
        print_generic_representation(&temp_rep);

        get_time(start_it);
        minimax_iteration_main(analysis_depth, UINT32_MAX, player_to_move, &pos, &move);
        player_to_move = !player_to_move;
        get_time(stop_it);

        _printf("Maximized score: %d      %llu\n", move.evaluation, get_time_diff_millis(stop_it, start_it));

        pos = move.best_field;
        print_field(&pos);
        _printf("\n");

        CHECK_WIN();

        _printf("pos: ");
        intern_to_generic_representation(&pos, &temp_rep);
        print_generic_representation(&temp_rep);

        get_time(start_it);
        minimax_iteration_main(analysis_depth, UINT32_MAX, player_to_move, &pos, &move);
        player_to_move = !player_to_move;
        get_time(stop_it);

        _printf("Minimized score: %d      %llu\n", move.evaluation, get_time_diff_millis(stop_it, start_it));

        pos = move.best_field;
        print_field(&pos);
        _printf("\n");

        CHECK_WIN();

#ifdef BRANCH_DEBUG
        clear_branch_tracker();
        for (int32_t i = 0; i < _total_branch_count; ++i)
        {
            _printf("branch=%4d (%s), total entries: %llu, cutoffs: %llu, cutoff%%=%f, recursion_cost=%llu\n", i + 1, cutoff_tracker[i].msg, cutoff_tracker[i].total_entries, cutoff_tracker[i].cutoff_entries, (double)cutoff_tracker[i].cutoff_entries * 100.0 / (double)cutoff_tracker[i].total_entries, cutoff_tracker[i].recursion_cost);
        }
#endif
    }
}

#undef CHECK_WIN

__attribute__((used, visibility("hidden"))) static void rnab_benchmark()
{
    // for (int i = 7; i < 70; ++i)
    // {
    //     for (int u = 0; u < 70; ++u)
    //     {
    //         field_t f;
    //         minimax_main_result_t r;
    //         init_field(&f, indexes[i], indexes[u]);
    //         minimax_iteration_main(16, UINT32_MAX, true, &f, &r);

    //         _printf("%d ", r.evaluation);
    //     }
    //     _printf("\n");
    // }
    // return;
    // simulate_game(&benchmark_array[0].position, true, 16);
    // return;

#ifdef BRANCH_DEBUG
    clear_branch_tracker();
#endif

    static TIME_TYPE start, stop;
    static generic_representation temp_field;
    static minimax_main_result_t ret;
    static field_t pos;

    get_time(start);
    for (int i = 0; i < sizeof(benchmark_array) / sizeof(benchmark_array[0]); ++i)
    {
        generic_representation_to_intern(&benchmark_array[i].position, &pos);
        print_field(&pos);

        RNAB_RESET_REVCACHE();
        minimax_iteration_main(benchmark_array[i].depth, UINT32_MAX, true, &pos, &ret);
        print_field(&ret.best_field);

        print_tt_stats();

        pos = reverse_field(&pos);
        print_field(&pos);

        RNAB_SET_REVCACHE();

        minimax_iteration_main(benchmark_array[i].depth, UINT32_MAX, false, &pos, &ret);
        print_field(&ret.best_field);

        print_tt_stats();

#ifdef BRANCH_DEBUG
        // for (int32_t i = 0; i < _total_branch_count; ++i)
        // {
        //     int64_t pct_whole = 0, pct_frac = 0;
        //     if (cutoff_tracker[i].total_entries > 0)
        //     {
        //         int64_t pct_int = cutoff_tracker[i].cutoff_entries * 10000000LL / cutoff_tracker[i].total_entries;
        //         pct_whole = pct_int / 100000;
        //         pct_frac = pct_int % 100000;
        //     }
        //     _printf("branch=%4d (%s), total entries: %lld, cutoffs: %lld, cutoff%%=%lld.%05lld, recursion_cost=%lld\n", i + 1, cutoff_tracker[i].msg, cutoff_tracker[i].total_entries, cutoff_tracker[i].cutoff_entries, pct_whole, pct_frac, cutoff_tracker[i].recursion_cost);
        // }
        // clear_branch_tracker();
#endif
    }
    get_time(stop);
    _printf("Compute time: %llu\n", get_time_diff_millis(stop, start));
}
#endif
