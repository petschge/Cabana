#include <Cabana_DeepCopy.hpp>
#include <Cabana_AoSoA.hpp>
#include <Cabana_MemberSlice.hpp>

#include <boost/test/unit_test.hpp>

//---------------------------------------------------------------------------//
// Check the data given a set of values.
template<class aosoa_type>
void checkDataMembers(
    aosoa_type aosoa,
    const float fval, const double dval, const int ival,
    const std::size_t dim_1, const std::size_t dim_2,
    const std::size_t dim_3, const std::size_t dim_4 )
{
    for ( auto idx = aosoa.begin(); idx != aosoa.end(); ++idx )
    {
        // Member 0.
        for ( std::size_t i = 0; i < dim_1; ++i )
            for ( std::size_t j = 0; j < dim_2; ++j )
                for ( std::size_t k = 0; k < dim_3; ++k )
                    BOOST_CHECK( aosoa.template get<0>( idx, i, j, k ) ==
                                 fval * (i+j+k) );

        // Member 1.
        BOOST_CHECK( aosoa.template get<1>( idx ) == ival );

        // Member 2.
        for ( std::size_t i = 0; i < dim_1; ++i )
            for ( std::size_t j = 0; j < dim_2; ++j )
                for ( std::size_t k = 0; k < dim_3; ++k )
                    for ( std::size_t l = 0; l < dim_4; ++l )
                        BOOST_CHECK( aosoa.template get<2>( idx, i, j, k, l ) ==
                                     fval * (i+j+k+l) );

        // Member 3.
        for ( std::size_t i = 0; i < dim_1; ++i )
            BOOST_CHECK( aosoa.template get<3>( idx, i ) == dval * i );

        // Member 4.
        for ( std::size_t i = 0; i < dim_1; ++i )
            for ( std::size_t j = 0; j < dim_2; ++j )
                BOOST_CHECK( aosoa.template get<4>( idx, i, j ) == dval * (i+j) );
    }
}

//---------------------------------------------------------------------------//
// Perform a deep copy test.
template<class DstMemorySpace, class SrcMemorySpace,
         class DstInnerArraySize, class SrcInnerArraySize>
void testDeepCopy()
{
    // Data dimensions.
    const std::size_t dim_1 = 3;
    const std::size_t dim_2 = 2;
    const std::size_t dim_3 = 4;
    const std::size_t dim_4 = 3;

    // Declare data types.
    using DataTypes =
        Cabana::MemberDataTypes<float[dim_1][dim_2][dim_3],
                                int,
                                float[dim_1][dim_2][dim_3][dim_4],
                                double[dim_1],
                                double[dim_1][dim_2]
                                >;

    // Declare the AoSoA typs.
    using DstAoSoA_t = Cabana::AoSoA<DataTypes,DstInnerArraySize,DstMemorySpace>;
    using SrcAoSoA_t = Cabana::AoSoA<DataTypes,SrcInnerArraySize,SrcMemorySpace>;

    // Create AoSoAs.
    std::size_t num_data = 357;
    DstAoSoA_t dst_aosoa( num_data );
    SrcAoSoA_t src_aosoa( num_data );

    // Initialize data with the rank accessors.
    float fval = 3.4;
    double dval = 1.23;
    int ival = 1;
    for ( auto idx = src_aosoa.begin(); idx != src_aosoa.end(); ++idx )
    {
        // Member 0.
        for ( std::size_t i = 0; i < dim_1; ++i )
            for ( std::size_t j = 0; j < dim_2; ++j )
                for ( std::size_t k = 0; k < dim_3; ++k )
                    src_aosoa.template get<0>( idx, i, j, k ) = fval * (i+j+k);

        // Member 1.
        src_aosoa.template get<1>( idx ) = ival;

        // Member 2.
        for ( std::size_t i = 0; i < dim_1; ++i )
            for ( std::size_t j = 0; j < dim_2; ++j )
                for ( std::size_t k = 0; k < dim_3; ++k )
                    for ( std::size_t l = 0; l < dim_4; ++l )
                        src_aosoa.template get<2>( idx, i, j, k, l ) = fval * (i+j+k+l);

        // Member 3.
        for ( std::size_t i = 0; i < dim_1; ++i )
            src_aosoa.template get<3>( idx, i ) = dval * i;

        // Member 4.
        for ( std::size_t i = 0; i < dim_1; ++i )
            for ( std::size_t j = 0; j < dim_2; ++j )
                src_aosoa.template get<4>( idx, i, j ) = dval * (i+j);
    }

    // Deep copy
    Cabana::deep_copy( dst_aosoa, src_aosoa );

    // Check values.
    checkDataMembers( dst_aosoa, fval, dval, ival, dim_1, dim_2, dim_3, dim_4 );
}

//---------------------------------------------------------------------------//
// TESTS
//---------------------------------------------------------------------------//
BOOST_AUTO_TEST_CASE( deep_copy_to_host_same_size_test )
{
    testDeepCopy<Kokkos::HostSpace,TEST_MEMSPACE,
                 Cabana::InnerArraySize<10>,Cabana::InnerArraySize<10>>();
}

//---------------------------------------------------------------------------//
BOOST_AUTO_TEST_CASE( deep_copy_from_host_same_size_test )
{
    testDeepCopy<TEST_MEMSPACE,Kokkos::HostSpace,
                 Cabana::InnerArraySize<10>,Cabana::InnerArraySize<10>>();
}

//---------------------------------------------------------------------------//
BOOST_AUTO_TEST_CASE( deep_copy_to_host_different_size_test )
{
    testDeepCopy<Kokkos::HostSpace,TEST_MEMSPACE,
                 Cabana::InnerArraySize<10>,Cabana::InnerArraySize<12>>();
    testDeepCopy<Kokkos::HostSpace,TEST_MEMSPACE,
                 Cabana::InnerArraySize<13>,Cabana::InnerArraySize<8>>();
}

//---------------------------------------------------------------------------//
BOOST_AUTO_TEST_CASE( deep_copy_from_host_different_size_test )
{
    testDeepCopy<TEST_MEMSPACE,Kokkos::HostSpace,
                 Cabana::InnerArraySize<10>,Cabana::InnerArraySize<12>>();
    testDeepCopy<TEST_MEMSPACE,Kokkos::HostSpace,
                 Cabana::InnerArraySize<13>,Cabana::InnerArraySize<8>>();
}

//---------------------------------------------------------------------------//
