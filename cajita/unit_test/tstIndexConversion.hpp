/****************************************************************************
 * Copyright (c) 2018-2023 by the Cabana authors                            *
 * All rights reserved.                                                     *
 *                                                                          *
 * This file is part of the Cabana library. Cabana is distributed under a   *
 * BSD 3-clause license. For the licensing terms see the LICENSE file in    *
 * the top-level directory.                                                 *
 *                                                                          *
 * SPDX-License-Identifier: BSD-3-Clause                                    *
 ****************************************************************************/

#include <Cajita_Array.hpp>
#include <Cajita_GlobalGrid.hpp>
#include <Cajita_GlobalMesh.hpp>
#include <Cajita_Halo.hpp>
#include <Cajita_IndexConversion.hpp>
#include <Cajita_IndexSpace.hpp>
#include <Cajita_LocalGrid.hpp>
#include <Cajita_LocalMesh.hpp>
#include <Cajita_Partitioner.hpp>
#include <Cajita_Types.hpp>

#include <gtest/gtest.h>

#include <mpi.h>

#include <array>
#include <numeric>

using namespace Cajita;

namespace Test
{
//---------------------------------------------------------------------------//
template <class EntityType>
void testConversion3d( const std::array<bool, 3>& is_dim_periodic )
{
    // Let MPI compute the partitioning for this test.
    DimBlockPartitioner<3> partitioner;

    // Create the global mesh.
    double cell_size = 0.23;
    std::array<int, 3> global_num_cell = { 51, 40, 37 };
    std::array<double, 3> global_low_corner = { 1.2, 3.3, -2.8 };
    std::array<double, 3> global_high_corner = {
        global_low_corner[0] + cell_size * global_num_cell[0],
        global_low_corner[1] + cell_size * global_num_cell[1],
        global_low_corner[2] + cell_size * global_num_cell[2] };
    auto global_mesh = createUniformGlobalMesh(
        global_low_corner, global_high_corner, global_num_cell );

    // Create the global grid.
    auto global_grid = createGlobalGrid( MPI_COMM_WORLD, global_mesh,
                                         is_dim_periodic, partitioner );

    // Create a local grid.
    int halo_width = 3;
    auto local_grid = createLocalGrid( global_grid, halo_width );

    // Create an array for global node indices.
    auto array_layout = createArrayLayout( local_grid, 3, EntityType() );
    auto global_index_array =
        createArray<int, TEST_MEMSPACE>( "global_indices", array_layout );
    auto index_view = global_index_array->view();

    // Fill the owned array with global indices.
    auto own_local_space =
        local_grid->indexSpace( Own(), EntityType(), Local() );
    auto own_global_space =
        local_grid->indexSpace( Own(), EntityType(), Global() );
    Kokkos::parallel_for(
        "fill_indices",
        createExecutionPolicy( own_global_space, TEST_EXECSPACE() ),
        KOKKOS_LAMBDA( const int i, const int j, const int k ) {
            int li = i - own_global_space.min( Dim::I ) +
                     own_local_space.min( Dim::I );
            int lj = j - own_global_space.min( Dim::J ) +
                     own_local_space.min( Dim::J );
            int lk = k - own_global_space.min( Dim::K ) +
                     own_local_space.min( Dim::K );
            index_view( li, lj, lk, Dim::I ) = i;
            index_view( li, lj, lk, Dim::J ) = j;
            index_view( li, lj, lk, Dim::K ) = k;
        } );

    // Gather to get the ghosted global indices.
    auto halo =
        createHalo( NodeHaloPattern<3>(), halo_width, *global_index_array );
    halo->gather( TEST_EXECSPACE(), *global_index_array );

    // Do a loop over ghosted local indices and fill with the index
    // conversion.
    auto global_l2g_array =
        createArray<int, TEST_MEMSPACE>( "global_indices", array_layout );
    auto l2g_view = global_l2g_array->view();
    auto ghost_local_space =
        local_grid->indexSpace( Ghost(), EntityType(), Local() );
    auto l2g = IndexConversion::createL2G( *local_grid, EntityType() );
    Kokkos::parallel_for(
        "fill_l2g",
        createExecutionPolicy( ghost_local_space, TEST_EXECSPACE() ),
        KOKKOS_LAMBDA( const int i, const int j, const int k ) {
            int gi, gj, gk;
            l2g( i, j, k, gi, gj, gk );
            l2g_view( i, j, k, Dim::I ) = gi;
            l2g_view( i, j, k, Dim::J ) = gj;
            l2g_view( i, j, k, Dim::K ) = gk;
        } );

    // Compare the results.
    auto index_view_host =
        Kokkos::create_mirror_view_and_copy( Kokkos::HostSpace(), index_view );
    auto l2g_view_host =
        Kokkos::create_mirror_view_and_copy( Kokkos::HostSpace(), l2g_view );
    auto check_results = [&]( const IndexSpace<3>& space )
    {
        for ( int i = space.min( Dim::I ); i < space.max( Dim::I ); ++i )
            for ( int j = space.min( Dim::J ); j < space.max( Dim::J ); ++j )
                for ( int k = space.min( Dim::K ); k < space.max( Dim::K );
                      ++k )
                    for ( int d = 0; d < 3; ++d )
                        EXPECT_EQ( l2g_view_host( i, j, k, d ),
                                   index_view_host( i, j, k, d ) );
    };
    check_results( own_local_space );
    for ( int i = -1; i < 2; ++i )
        for ( int j = -1; j < 2; ++j )
            for ( int k = -1; k < 2; ++k )
                if ( local_grid->neighborRank( i, j, k ) >= 0 )
                    check_results( local_grid->sharedIndexSpace(
                        Ghost(), EntityType(), i, j, k ) );
}

//---------------------------------------------------------------------------//
template <class EntityType>
void testConversion2d( const std::array<bool, 2>& is_dim_periodic )
{
    // Let MPI compute the partitioning for this test.
    DimBlockPartitioner<2> partitioner;

    // Create the global mesh.
    double cell_size = 0.23;
    std::array<int, 2> global_num_cell = { 51, 40 };
    std::array<double, 2> global_low_corner = { 1.2, 3.3 };
    std::array<double, 2> global_high_corner = {
        global_low_corner[0] + cell_size * global_num_cell[0],
        global_low_corner[1] + cell_size * global_num_cell[1] };
    auto global_mesh = createUniformGlobalMesh(
        global_low_corner, global_high_corner, global_num_cell );

    // Create the global grid.
    auto global_grid = createGlobalGrid( MPI_COMM_WORLD, global_mesh,
                                         is_dim_periodic, partitioner );

    // Create a local grid.
    int halo_width = 3;
    auto local_grid = createLocalGrid( global_grid, halo_width );

    // Create an array for global node indices.
    auto array_layout = createArrayLayout( local_grid, 2, EntityType() );
    auto global_index_array =
        createArray<int, TEST_MEMSPACE>( "global_indices", array_layout );
    auto index_view = global_index_array->view();

    // Fill the owned array with global indices.
    auto own_local_space =
        local_grid->indexSpace( Own(), EntityType(), Local() );
    auto own_global_space =
        local_grid->indexSpace( Own(), EntityType(), Global() );
    Kokkos::parallel_for(
        "fill_indices",
        createExecutionPolicy( own_global_space, TEST_EXECSPACE() ),
        KOKKOS_LAMBDA( const int i, const int j ) {
            int li = i - own_global_space.min( Dim::I ) +
                     own_local_space.min( Dim::I );
            int lj = j - own_global_space.min( Dim::J ) +
                     own_local_space.min( Dim::J );
            index_view( li, lj, Dim::I ) = i;
            index_view( li, lj, Dim::J ) = j;
        } );

    // Gather to get the ghosted global indices.
    auto halo =
        createHalo( NodeHaloPattern<2>(), halo_width, *global_index_array );
    halo->gather( TEST_EXECSPACE(), *global_index_array );

    // Do a loop over ghosted local indices and fill with the index
    // conversion.
    auto global_l2g_array =
        createArray<int, TEST_MEMSPACE>( "global_indices", array_layout );
    auto l2g_view = global_l2g_array->view();
    auto ghost_local_space =
        local_grid->indexSpace( Ghost(), EntityType(), Local() );
    auto l2g = IndexConversion::createL2G( *local_grid, EntityType() );
    Kokkos::parallel_for(
        "fill_l2g",
        createExecutionPolicy( ghost_local_space, TEST_EXECSPACE() ),
        KOKKOS_LAMBDA( const int i, const int j ) {
            int gi, gj;
            l2g( i, j, gi, gj );
            l2g_view( i, j, Dim::I ) = gi;
            l2g_view( i, j, Dim::J ) = gj;
        } );

    // Compare the results.
    auto index_view_host =
        Kokkos::create_mirror_view_and_copy( Kokkos::HostSpace(), index_view );
    auto l2g_view_host =
        Kokkos::create_mirror_view_and_copy( Kokkos::HostSpace(), l2g_view );
    auto check_results = [&]( const IndexSpace<2>& space )
    {
        for ( int i = space.min( Dim::I ); i < space.max( Dim::I ); ++i )
            for ( int j = space.min( Dim::J ); j < space.max( Dim::J ); ++j )
                for ( int d = 0; d < 2; ++d )
                    EXPECT_EQ( l2g_view_host( i, j, d ),
                               index_view_host( i, j, d ) );
    };
    check_results( own_local_space );
    for ( int i = -1; i < 2; ++i )
        for ( int j = -1; j < 2; ++j )
            if ( local_grid->neighborRank( i, j ) >= 0 )
                check_results( local_grid->sharedIndexSpace(
                    Ghost(), EntityType(), i, j ) );
}

//---------------------------------------------------------------------------//
// 3d
TEST( index_conversion, node_periodic_3d_test )
{
    testConversion3d<Node>( { { true, true, true } } );
}
TEST( index_conversion, cell_periodic_3d_test )
{
    testConversion3d<Cell>( { { true, true, true } } );
}
TEST( index_conversion, face_i_periodic_3d_test )
{
    testConversion3d<Face<Dim::I>>( { { true, true, true } } );
}
TEST( index_conversion, face_j_periodic_3d_test )
{
    testConversion3d<Face<Dim::J>>( { { true, true, true } } );
}
TEST( index_conversion, face_k_periodic_3d_test )
{
    testConversion3d<Face<Dim::K>>( { { true, true, true } } );
}
TEST( index_conversion, edge_i_periodic_3d_test )
{
    testConversion3d<Edge<Dim::I>>( { { true, true, true } } );
}
TEST( index_conversion, edge_j_periodic_3d_test )
{
    testConversion3d<Edge<Dim::J>>( { { true, true, true } } );
}
TEST( index_conversion, edge_k_periodic_3d_test )
{
    testConversion3d<Edge<Dim::K>>( { { true, true, true } } );
}

TEST( index_conversion, node_not_periodic_3d_test )
{
    testConversion3d<Node>( { { false, false, false } } );
}
TEST( index_conversion, cell_not_periodic_3d_test )
{
    testConversion3d<Cell>( { { false, false, false } } );
}
TEST( index_conversion, face_i_not_periodic_3d_test )
{
    testConversion3d<Face<Dim::I>>( { { false, false, false } } );
}
TEST( index_conversion, face_j_not_periodic_3d_test )
{
    testConversion3d<Face<Dim::J>>( { { false, false, false } } );
}
TEST( index_conversion, face_k_not_periodic_3d_test )
{
    testConversion3d<Face<Dim::K>>( { { false, false, false } } );
}
TEST( index_conversion, edge_i_not_periodic_3d_test )
{
    testConversion3d<Edge<Dim::I>>( { { false, false, false } } );
}
TEST( index_conversion, edge_j_not_periodic_3d_test )
{
    testConversion3d<Edge<Dim::J>>( { { false, false, false } } );
}
TEST( index_conversion, edge_k_not_periodic_3d_test )
{
    testConversion3d<Edge<Dim::K>>( { { false, false, false } } );
}

//---------------------------------------------------------------------------//
// 2d
TEST( index_conversion, node_periodic_2d_test )
{
    testConversion2d<Node>( { { true, true } } );
}
TEST( index_conversion, cell_periodic_2d_test )
{
    testConversion2d<Cell>( { { true, true } } );
}
TEST( index_conversion, face_i_periodic_2d_test )
{
    testConversion2d<Face<Dim::I>>( { { true, true } } );
}
TEST( index_conversion, face_j_periodic_2d_test )
{
    testConversion2d<Face<Dim::J>>( { { true, true } } );
}

TEST( index_conversion, node_not_periodic_2d_test )
{
    testConversion2d<Node>( { { false, false } } );
}
TEST( index_conversion, cell_not_periodic_2d_test )
{
    testConversion2d<Cell>( { { false, false } } );
}
TEST( index_conversion, face_i_not_periodic_2d_test )
{
    testConversion2d<Face<Dim::I>>( { { false, false } } );
}
TEST( index_conversion, face_j_not_periodic_2d_test )
{
    testConversion2d<Face<Dim::J>>( { { false, false } } );
}

//---------------------------------------------------------------------------//

} // end namespace Test
