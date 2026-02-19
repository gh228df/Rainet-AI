#include "rnab_impl.hpp"

extern minimax_main_result_t minimax_iteration_main_scalar(const int depth, int alpha, int beta, const bool player, field_t &position);
extern minimax_main_result_t minimax_main_scalar(const int depth, int alpha, int beta, const bool player, field_t &position);
extern possible_moves_t possible_moves_scalar(const field_t &position, const bool player);

#if defined(__x86_64__)

extern minimax_main_result_t minimax_iteration_main_avx512f(const int depth, int alpha, int beta, const bool player, field_t &position);
extern minimax_main_result_t minimax_iteration_main_avx2(const int depth, int alpha, int beta, const bool player, field_t &position);
extern minimax_main_result_t minimax_iteration_main_avx(const int depth, int alpha, int beta, const bool player, field_t &position);
extern minimax_main_result_t minimax_iteration_main_sse4_2(const int depth, int alpha, int beta, const bool player, field_t &position);

extern minimax_main_result_t minimax_main_avx512f(const int depth, int alpha, int beta, const bool player, field_t &position);
extern minimax_main_result_t minimax_main_avx2(const int depth, int alpha, int beta, const bool player, field_t &position);
extern minimax_main_result_t minimax_main_avx(const int depth, int alpha, int beta, const bool player, field_t &position);
extern minimax_main_result_t minimax_main_sse4_2(const int depth, int alpha, int beta, const bool player, field_t &position);

extern possible_moves_t possible_moves_avx512f(const field_t &position, const bool player);
extern possible_moves_t possible_moves_avx2(const field_t &position, const bool player);
extern possible_moves_t possible_moves_avx(const field_t &position, const bool player);
extern possible_moves_t possible_moves_sse4_2(const field_t &position, const bool player);

minimax_main_result_t (*minimax_iteration_main)(const int depth, int alpha, int beta, const bool player, field_t &position) = ((__builtin_cpu_supports("avx512f") && __builtin_cpu_supports("avx512vl") && __builtin_cpu_supports("avx512bw") && __builtin_cpu_supports("avx512dq")) ? minimax_iteration_main_avx512f : ((__builtin_cpu_supports("avx2")) ? minimax_iteration_main_avx2 : ((__builtin_cpu_supports("avx")) ? minimax_iteration_main_avx : ((__builtin_cpu_supports("sse4.2")) ? minimax_iteration_main_sse4_2 : minimax_iteration_main_scalar))));

minimax_main_result_t (*minimax_main)(const int depth, int alpha, int beta, const bool player, field_t &position) = ((__builtin_cpu_supports("avx512f") && __builtin_cpu_supports("avx512vl") && __builtin_cpu_supports("avx512bw") && __builtin_cpu_supports("avx512dq")) ? minimax_main_avx512f : ((__builtin_cpu_supports("avx2")) ? minimax_main_avx2 : ((__builtin_cpu_supports("avx")) ? minimax_main_avx : ((__builtin_cpu_supports("sse4.2")) ? minimax_main_sse4_2 : minimax_main_scalar))));

possible_moves_t (*possible_moves)(const field_t &position, const bool player) = ((__builtin_cpu_supports("avx512f") && __builtin_cpu_supports("avx512vl") && __builtin_cpu_supports("avx512bw") && __builtin_cpu_supports("avx512dq")) ? possible_moves_avx512f : ((__builtin_cpu_supports("avx2")) ? possible_moves_avx2 : ((__builtin_cpu_supports("avx")) ? possible_moves_avx : ((__builtin_cpu_supports("sse4.2")) ? possible_moves_sse4_2 : possible_moves_scalar))));

#else

minimax_main_result_t (*minimax_iteration_main)(const int depth, int alpha, int beta, const bool player, field_t &position) = minimax_iteration_main_scalar;
minimax_main_result_t (*minimax_main)(const int depth, int alpha, int beta, const bool player, field_t &position) = minimax_main_scalar;
possible_moves_t (*possible_moves)(const field_t &position, const bool player) = possible_moves_scalar;

#endif
