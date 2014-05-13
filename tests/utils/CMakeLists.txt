build_and_run_test(test_strlcat.c utils)
build_and_run_test(test_strlcpy.c utils)
build_and_run_test(test_double_poly.c "utils;tests")
build_and_run_test(test_gmp_aux.c "utils;tests")
build_and_run_test(test_gcd.c "utils;tests")
build_and_run_test(test_getprime.c utils)
build_and_run_test(test_memusage.c utils)
build_and_run_test(test_roots_mod.c utils 100 1050)
build_and_run_test(test_omega.c utils)
build_and_run_test(test_timing.c utils)
build_and_run_test(test_rootfinder.c "utils;tests")
build_and_run_test(test_mod_ul.c "utils;tests" 1000)
build_and_run_test(test_modredc_ul.c "utils;tests" 1000)
build_and_run_test(test_modredc_15ul.c "utils;tests" 1000)
build_and_run_test(test_modredc_2ul2.c "utils;tests" 1000)
build_and_run_test(test_sm_utils.c utils ${CMAKE_CURRENT_SOURCE_DIR}/test_sm_utils.data)
build_and_run_test(test_mpz_poly.c "utils;tests")
build_and_run_test(test_cado_poly.c "utils;tests")
build_and_run_test(test_relation.c "utils;tests")
build_and_run_test(test_cache.c "utils")
build_and_run_test(test_usp.c "utils;tests")
build_and_run_test(test_bit_vector.c "utils;tests")
build_and_run_test(test_discriminant.c "utils;tests")
build_and_run_test(test_intinv.c "utils;tests")
build_and_run_test(test_modul_poly.c "utils;tests")
build_and_run_test(test_modredcul_batchinv.c "utils;tests")
build_and_run_test(test_modredcul_batchinv_ul.c "utils;tests")
build_and_run_test(test_dllist.c "tests")