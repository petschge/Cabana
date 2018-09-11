#ifndef CABANA_AOSOA_HPP
#define CABANA_AOSOA_HPP

#include <Cabana_MemberDataTypes.hpp>
#include <Cabana_Slice.hpp>
#include <Cabana_Tuple.hpp>
#include <Cabana_Types.hpp>
#include <Cabana_SoA.hpp>
#include <impl/Cabana_Index.hpp>
#include <impl/Cabana_PerformanceTraits.hpp>

#include <Kokkos_Core.hpp>

#include <type_traits>
#include <cmath>
#include <cstdlib>
#include <string>

namespace Cabana
{
//---------------------------------------------------------------------------//
/*!
  \class AoSoA

  \brief Array-of-Struct-of-Arrays

  A AoSoA represents tuples and their data via an array-of-structs-of-arrays.

  This class has both required and optional template parameters.  The
  \c DataType parameter must always be provided, and must always be
  first. The parameters \c Arg1Type, \c Arg2Type, and \c Arg3Type are
  placeholders for different template parameters.  The default value
  of the fifth template parameter \c Specialize suffices for most use
  cases.  When explaining the template parameters, we won't refer to
  \c Arg1Type, \c Arg2Type, and \c Arg3Type; instead, we will refer
  to the valid categories of template parameters, in whatever order
  they may occur.

  \tparam DataType (required) Specifically this must be an instance of
  \c MemberDataTypes with the data layout of the structs. For example:
  \code
  using DataType = MemberDataTypes<double[3][3],double[3],int>;
  \endcode
  would define an AoSoA where each tuple had a 3x3 matrix of doubles, a
  3-vector of doubles, and an integer. The AoSoA is then templated on this
  sequence of types. In general, put larger datatypes first in the
  MemberDataType parameter pack (i.e. matrices and vectors) and group members
  of the same type together to achieve the smallest possible memory footprint
  based on compiler-generated padding.

  \tparam MemorySpace (required) The memory space.

  \tparam VectorLength (optional) The vector length within the structs of
  the AoSoA. If not specified, this defaults to the preferred layout for the
  <tt>MemorySpace</tt>.
 */
template<class DataTypes,
         class MemorySpace,
         int VectorLength = Impl::PerformanceTraits<
             typename MemorySpace::kokkos_execution_space>::vector_length>
class AoSoA
{
  public:

    // AoSoA type.
    using aosoa_type = AoSoA<DataTypes,MemorySpace,VectorLength>;

    // Member data types.
    using member_types = DataTypes;

    // Memory space.
    using memory_space = MemorySpace;

    // Vector length (size of the arrays held by the structs).
    static constexpr int vector_length = VectorLength;

    // SoA type.
    using soa_type = SoA<vector_length,member_types>;

    // Managed data view.
    using soa_view = Kokkos::View<soa_type*,typename memory_space::kokkos_memory_space>;

    // Number of member types.
    static constexpr std::size_t number_of_members = member_types::size;

    // The maximum rank supported for member types.
    static constexpr int max_supported_rank = 4;

    // Index type.
    using index_type = Impl::Index<vector_length>;

    // Tuple type.
    using tuple_type = Tuple<member_types>;

    // Member data type at a given index M. Note this is the user-defined
    // member data type - not the potentially transformed type actually stored
    // by the structs (SoAs) to achieve a given layout.
    template<std::size_t Field>
    using member_data_type =
        typename MemberDataTypeAtIndex<Field,member_types>::type;

    // Struct member array element value type at a given index M.
    template<std::size_t Field>
    using member_value_type =
        typename std::remove_all_extents<member_data_type<Field> >::type;

    // Struct member array element pointer type at a given index M.
    template<std::size_t Field>
    using member_pointer_type =
        typename std::add_pointer<member_value_type<Field> >::type;

  public:

    /*!
      \brief Default constructor.

      The container size is zero and no memory is allocated.
    */
    AoSoA()
        : _size( 0 )
        , _capacity( 0 )
        , _num_soa( 0 )
    {}

    /*!
      \brief Allocate a container with n tuples.

      \param n The number of tuples in the container.
    */
    explicit AoSoA( const int n )
        : _size( n )
        , _capacity( 0 )
        , _num_soa( 0 )
    {
        resize( _size );
    }

    /*!
      \brief Returns the number of tuples in the container.

      \return The number of tuples in the container.

      This is the number of actual objects held in the container, which is not
      necessarily equal to its storage capacity.
    */
    KOKKOS_FUNCTION
    int size() const { return _size; }

    /*!
      \brief Returns the size of the storage space currently allocated for the
      container, expressed in terms of tuples.

      \return The capacity of the container.

      This capacity is not necessarily equal to the container size. It can be
      equal or greater, with the extra space allowing to accommodate for
      growth without the need to reallocate on each insertion.

      Notice that this capacity does not suppose a limit on the size of the
      container. When this capacity is exhausted and more is needed, it is
      automatically expanded by the container (reallocating it storage space).

      The capacity of a container can be explicitly altered by calling member
      reserve.
    */
    KOKKOS_FUNCTION
    int capacity() const { return _capacity; }

    /*!
      \brief Resizes the container so that it contains n tuples.

      If n is smaller than the current container size, the content is reduced
      to its first n tuples.

      If n is greater than the current container size, the content is expanded
      by inserting at the end as many tuples as needed to reach a size of n.

      If n is also greater than the current container capacity, an automatic
      reallocation of the allocated storage space takes place.

      Notice that this function changes the actual content of the container by
      inserting or erasing tuples from it.
    */
    void resize( const int n )
    {
        // Reserve memory if needed.
        reserve( n );

        // Update the sizes of the data. This is potentially different than
        // the amount of allocated data.
        _size = n;
        _num_soa = std::floor( n / vector_length );
        if ( 0 < n % vector_length ) ++_num_soa;
    }

    /*!
      \brief Requests that the container capacity be at least enough to contain n
      tuples.

      If n is greater than the current container capacity, the function causes
      the container to reallocate its storage increasing its capacity to n (or
      greater).

      In all other cases, the function call does not cause a reallocation and
      the container capacity is not affected.

      This function has no effect on the container size and cannot alter its
      tuples.
    */
    void reserve( const int n )
    {
        // If we aren't asking for more memory then we have nothing to do.
        if ( n <= _capacity ) return;

        // Figure out the new capacity.
        int num_soa_alloc = std::floor( n / vector_length );
        if ( 0 < n % vector_length ) ++num_soa_alloc;

        // If we aren't asking for any more SoA objects then we still have
        // nothing to do.
        if ( num_soa_alloc <= _num_soa ) return;

        // Assign the new capacity.
        _capacity = num_soa_alloc * vector_length;

        // If we need more SoA objects then resize.
        Kokkos::resize( _data, num_soa_alloc );

        // Get new pointers and strides for the members.
        storePointersAndStrides(
            std::integral_constant<std::size_t,number_of_members-1>() );
    }

    /*!
      \brief Get the number of structs-of-arrays in the container.

      \return The number of structs-of-arrays in the container.
    */
    KOKKOS_INLINE_FUNCTION
    int numSoA() const { return _num_soa; }

    /*!
      \brief Get the size of the data array at a given struct member index.

      \param s The struct index to get the array size for.

      \return The size of the array at the given struct index.
    */
    KOKKOS_INLINE_FUNCTION
    int arraySize( const int s ) const
    {
        return
            ( s < _num_soa - 1 ) ? vector_length : ( _size % vector_length );
    }

    /*!
      \brief Get the SoA at a given index.

      \param s The SoA index.

      \return The SoA at the given index.
    */
    KOKKOS_FORCEINLINE_FUNCTION
    soa_type& access( const int s ) const
    { return _data(s); }

    /*!
      \brief Get a tuple at a given index.

      \param idx The index to get the tuple from.

      \return A tuple containing a copy of the data at the given index.
    */
    KOKKOS_INLINE_FUNCTION
    tuple_type getTuple( const int idx ) const
    {
        tuple_type tpl;
        Impl::tupleCopy(
            tpl, 0, _data(index_type::s(idx)), index_type::i(idx) );
        return tpl;
    }

    /*!
      \brief Set a tuple at a given index.

      \param idx The index to set the tuple at.

      \param tuple The tuple to get the data from.
    */
    KOKKOS_INLINE_FUNCTION
    void setTuple( const int idx,
                   const tuple_type& tpl ) const
    {
        Impl::tupleCopy(
            _data(index_type::s(idx)), index_type::i(idx), tpl, 0 );
    }

    /*!
      \brief Get an unmanaged slice of a tuple member with default memory
      access.
      \param The tag identifying which member to get a slice of.
      \return The member slice.
    */
    template<std::size_t Member>
    Slice<member_data_type<Member>,
          memory_space,
          DefaultAccessMemory,
          vector_length>
    slice( MemberTag<Member> ) const
    {
        return
            Slice<member_data_type<Member>,
                  memory_space,
                  DefaultAccessMemory,
                  vector_length>(
                      (member_pointer_type<Member>) _pointers[Member],
                      _size, _strides[Member], _num_soa );
    }

    /*!
      \brief Get an un-typed raw pointer to the entire data block.
      \return An un-typed raw-pointer to the entire data block.
    */
    void* ptr() const
    { return _data.data(); }

  private:

    // Store the pointers and strides for each member element.
    template<std::size_t N>
    void assignPointersAndStrides()
    {
        static_assert( 0 <= N && N < number_of_members,
                       "Static loop out of bounds!" );
        _pointers[N] = _data(0).template ptr<N>();
        static_assert( 0 ==
                       sizeof(soa_type) % sizeof(member_value_type<N>),
                       "Stride cannot be calculated for misaligned memory!" );
        _strides[N] = sizeof(soa_type) / sizeof(member_value_type<N>);
    }

    // Static loop through each member element to extract pointers and strides.
    template<std::size_t N>
    void storePointersAndStrides( std::integral_constant<std::size_t,N> )
    {
        assignPointersAndStrides<N>();
        storePointersAndStrides( std::integral_constant<std::size_t,N-1>() );
    }

    void storePointersAndStrides( std::integral_constant<std::size_t,0> )
    {
        assignPointersAndStrides<0>();
    }

  private:

    // Total number of tuples in the container.
    int _size;

    // Allocated number of tuples in all arrays in all structs.
    int _capacity;

    // Number of structs-of-arrays in the array.
    int _num_soa;

    // Structs-of-Arrays managed data. This Kokkos View manages the block of
    // memory owned by this class such that the copy constructor and
    // assignment operator for this class perform a shallow and reference
    // counted copy of the data.
    soa_view _data;

    // Pointers to the first element of each member.
    void* _pointers[number_of_members];

    // Strides for each member. Note that these strides are computed in the
    // context of the *value_type* of each member.
    int _strides[number_of_members];
};

//---------------------------------------------------------------------------//
// Static type checker.
template<class >
struct is_aosoa : public std::false_type {};

template<class DataTypes, class MemorySpace, int VectorLength>
struct is_aosoa<AoSoA<DataTypes,MemorySpace,VectorLength> >
    : public std::true_type {};

template<class DataTypes, class MemorySpace, int VectorLength>
struct is_aosoa<const AoSoA<DataTypes,MemorySpace,VectorLength> >
    : public std::true_type {};

//---------------------------------------------------------------------------//

} // end namespace Cabana

#endif // CABANA_AOSOA_HPP
