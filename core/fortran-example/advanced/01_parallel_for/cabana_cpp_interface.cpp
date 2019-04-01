#include <Cabana_Parallel.hpp>
#include <Cabana_ExecutionPolicy.hpp>
#include <Cabana_Core.hpp>
#include <Kokkos_Core.hpp>

#include <chrono>
#include <cstdlib>
#include <iostream>

// Whether to use the GPU version
#ifndef USE_GPU
#define USE_GPU 0
#endif

// If using the CPU version, whether to use OpenMP
#ifndef USE_OMP
#define USE_OMP 1
#endif

// Declare the memory and execution spaces.
#if USE_GPU == 1
using MemorySpace = Cabana::CudaUVMSpace;
using ExecutionSpace = Kokkos::Cuda;
#else
using MemorySpace = Cabana::HostSpace;
#if USE_OMP == 1
using ExecutionSpace = Kokkos::OpenMP;
#else
using ExecutionSpace = Kokkos::Serial;
#endif
#endif

// Most particle routines can be written as a loop over particles.
// In the GPU case, launch a parallel_for over particles
// In the CPU case, launch a parallel_for over vectors
//   The vector loop is currently inclusive of the ends, so if you ask to operate over
//   particles 15-33 and your vector length is 16, you will operate over particle 1-48.

struct local_particle_struct_t {
    double ph[6][VEC_LEN];
    double ct[3][VEC_LEN];
    long long int gid[VEC_LEN];
};

#if USE_GPU==1
#define PARTICLE_OP(C_FUNC,F_FUNC) \
  extern "C" int C_FUNC( int sp, int np ); \
  extern "C" CABANA_FUNCTION void F_FUNC(local_particle_struct_t*, int, int); \
  int C_FUNC(int start_pt, int num_particle) \
  { \
      auto* p_loc = (local_particle_struct_t*)(particles->ptr()); \
      int num_vecs = (num_particle + VEC_LEN - 1) / VEC_LEN; \
      auto local_lambda = KOKKOS_LAMBDA( const int idx ) \
      { \
         F_FUNC(p_loc, num_vecs, idx); \
      }; \
      Kokkos::RangePolicy<ExecutionSpace> range_policy_vec( start_pt-1, num_particle ); \
      Kokkos::parallel_for( range_policy_vec, local_lambda, "example_op" ); \
      return 0; \
  }

#else
#define PARTICLE_OP(C_FUNC,F_FUNC) \
extern "C" int C_FUNC( int sp, int np ); \
extern "C" CABANA_FUNCTION void F_FUNC(local_particle_struct_t*, int, int); \
int C_FUNC(int sp, int num_particle) \
{ \
    auto* p_loc = (local_particle_struct_t*)(particles->ptr()); \
    int num_vecs = (num_particle + VEC_LEN - 1) / VEC_LEN; \
    int start_vec = sp / VEC_LEN - 1; \
    int one_vector = 1; \
    auto local_lambda = KOKKOS_LAMBDA( const int idx ) \
    { \
       F_FUNC(p_loc+idx, one_vector, idx); \
    }; \
    Kokkos::RangePolicy<ExecutionSpace> range_policy_vec( start_vec, num_vecs ); \
    Kokkos::parallel_for( range_policy_vec, local_lambda, "example_op" ); \
    return 0; \
}

#endif

using ParticleDataTypes = Cabana::MemberTypes<double[6],double[3],long long int>;

// Set the type and memory space for the particle AoSoA.
using ParticleList = Cabana::AoSoA<ParticleDataTypes,MemorySpace,VEC_LEN>;

// Wrapper for particle allocation
extern "C" int particle_allocation( int np );

ParticleList* particles;

// initialize structure size
int particle_allocation(int num_particle)
{
    // Set the particle list size
    particles = new ParticleList ( num_particle );
    particles->resize( num_particle );

    return 0;
}

// Define the function to be called by Fortran main
extern "C" int parallel_for_example( int sp, int ep );

// Define the kernel that will be called inside the parallel_for
extern "C" CABANA_FUNCTION void parallel_for_example_f( int );

int parallel_for_example( int start_pt, int end_pt)
{
  auto local_lambda = KOKKOS_LAMBDA( const int idx )
  {
     parallel_for_example_f(idx);
  };
  Kokkos::RangePolicy<ExecutionSpace> range_policy_vec( start_pt-1, end_pt );
  Kokkos::parallel_for( range_policy_vec, local_lambda, "example_op" );
  return 0;
}

PARTICLE_OP(particle_initialization,particle_initialization_f)

// Wrappers for C++ routines that will need to be called by the Fortran code.
extern "C" void cabana_initialize( void );
extern "C" void cabana_finalize( void );
extern "C" void kokkos_fence( void );

void cabana_initialize() {
  Kokkos::initialize();
}

void cabana_finalize( void ) {
  Kokkos::finalize();
}

void kokkos_fence() {
  Kokkos::fence();
}
