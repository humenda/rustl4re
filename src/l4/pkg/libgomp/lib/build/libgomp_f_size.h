#define YY(n, v) char foo_##n[v];

YY(OMP_LOCK_SIZE,  sizeof (omp_lock_t));
YY(OMP_LOCK_ALIGN,  __alignof (omp_lock_t));
YY(OMP_NEST_LOCK_SIZE,  sizeof (omp_nest_lock_t));
YY(OMP_NEST_LOCK_ALIGN,  __alignof (omp_nest_lock_t));
YY(sizeof_star_omp_lock_arg_t,  sizeof (*(omp_lock_arg_t) 0));
YY(sizeof_star_omp_nest_lock_arg_t, sizeof (*(omp_nest_lock_arg_t) 0));
YY(OMP_LOCK_25_SIZE,  sizeof (omp_lock_25_t));
YY(OMP_LOCK_25_ALIGN,  __alignof (omp_lock_25_t));
YY(OMP_NEST_LOCK_25_SIZE,  sizeof (omp_nest_lock_25_t));
YY(OMP_NEST_LOCK_25_ALIGN,  __alignof (omp_nest_lock_25_t));
YY(sizeof_star_omp_lock_25_arg_t,  sizeof (*(omp_lock_25_arg_t) 0));
YY(sizeof_star_omp_nest_lock_25_arg_t,  sizeof (*(omp_nest_lock_25_arg_t) 0));

