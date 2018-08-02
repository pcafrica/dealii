// ---------------------------------------------------------------------
//
// Copyright (C) 2017 - 2018 by the deal.II authors
//
// This file is part of the deal.II library.
//
// The deal.II library is free software; you can use it, redistribute
// it, and/or modify it under the terms of the GNU Lesser General
// Public License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
// The full text of the license can be found in the file LICENSE.md at
// the top level directory of deal.II.
//
// ---------------------------------------------------------------------


#include <deal.II/base/config.h>

#include <deal.II/base/hdf5.h>

#include <deal.II/lac/full_matrix.h>

#include <hdf5.h>

#include <memory>
#include <numeric>
#include <vector>

DEAL_II_NAMESPACE_OPEN

namespace HDF5

{
  namespace internal
  {
    template <typename T>
    std::shared_ptr<hid_t>
    get_hdf5_datatype()
    {
      std::shared_ptr<hid_t> t_type;
      if (std::is_same<T, float>::value)
        {
          t_type  = std::shared_ptr<hid_t>(new hid_t);
          *t_type = H5T_NATIVE_FLOAT;
        }
      else if (std::is_same<T, double>::value)
        {
          t_type  = std::shared_ptr<hid_t>(new hid_t);
          *t_type = H5T_NATIVE_DOUBLE;
        }
      else if (std::is_same<T, long double>::value)
        {
          t_type  = std::shared_ptr<hid_t>(new hid_t);
          *t_type = H5T_NATIVE_LDOUBLE;
        }
      else if (std::is_same<T, int>::value)
        {
          t_type  = std::shared_ptr<hid_t>(new hid_t);
          *t_type = H5T_NATIVE_INT;
        }
      else if (std::is_same<T, unsigned int>::value)
        {
          t_type  = std::shared_ptr<hid_t>(new hid_t);
          *t_type = H5T_NATIVE_UINT;
        }
      else if (std::is_same<T, std::complex<float>>::value)
        {
          t_type  = std::shared_ptr<hid_t>(new hid_t, [](auto pointer) {
            // Relase the HDF5 resource
            H5Tclose(*pointer);
            delete pointer;
          });
          *t_type = H5Tcreate(H5T_COMPOUND, sizeof(std::complex<float>));
          //  The C++ standards committee agreed to mandate that the storage
          //  format used for the std::complex type be binary-compatible with
          //  the C99 type, i.e. an array T[2] with consecutive real [0] and
          //  imaginary [1] parts.
          H5Tinsert(*t_type, "r", 0, H5T_NATIVE_FLOAT);
          H5Tinsert(*t_type, "i", sizeof(float), H5T_NATIVE_FLOAT);
        }
      else if (std::is_same<T, std::complex<double>>::value)
        {
          t_type  = std::shared_ptr<hid_t>(new hid_t, [](auto pointer) {
            // Relase the HDF5 resource
            H5Tclose(*pointer);
            delete pointer;
          });
          *t_type = H5Tcreate(H5T_COMPOUND, sizeof(std::complex<double>));
          //  The C++ standards committee agreed to mandate that the storage
          //  format used for the std::complex type be binary-compatible with
          //  the C99 type, i.e. an array T[2] with consecutive real [0] and
          //  imaginary [1] parts.
          H5Tinsert(*t_type, "r", 0, H5T_NATIVE_DOUBLE);
          H5Tinsert(*t_type, "i", sizeof(double), H5T_NATIVE_DOUBLE);
        }
      else if (std::is_same<T, std::complex<long double>>::value)
        {
          t_type  = std::shared_ptr<hid_t>(new hid_t, [](auto pointer) {
            // Relase the HDF5 resource
            H5Tclose(*pointer);
            delete pointer;
          });
          *t_type = H5Tcreate(H5T_COMPOUND, sizeof(std::complex<long double>));
          //  The C++ standards committee agreed to mandate that the storage
          //  format used for the std::complex type be binary-compatible with
          //  the C99 type, i.e. an array T[2] with consecutive real [0] and
          //  imaginary [1] parts.
          H5Tinsert(*t_type, "r", 0, H5T_NATIVE_LDOUBLE);
          H5Tinsert(*t_type, "i", sizeof(long double), H5T_NATIVE_LDOUBLE);
        }
      else
        {
          Assert(false, ExcInternalError());
        }
      return t_type;
    }
  } // namespace internal


  HDF5Object::HDF5Object(const std::string name, bool mpi)
    : name(name)
    , mpi(mpi)
  {}

  template <typename T>
  T
  HDF5Object::attr(const std::string attr_name) const
  {
    std::shared_ptr<hid_t> t_type = internal::get_hdf5_datatype<T>();
    T                      value;
    hid_t                  attr;


    attr = H5Aopen(*hdf5_reference, attr_name.data(), H5P_DEFAULT);
    H5Aread(attr, *t_type, &value);
    H5Aclose(attr);
    return value;
  }

  template <>
  bool
  HDF5Object::attr(const std::string attr_name) const
  {
    // The enum field generated by h5py can be casted to int
    int   int_value;
    hid_t attr;
    attr = H5Aopen(*hdf5_reference, attr_name.data(), H5P_DEFAULT);
    H5Aread(attr, H5T_NATIVE_INT, &int_value);
    H5Aclose(attr);
    // The int can be casted to a bool
    bool bool_value = (bool)int_value;
    return bool_value;
  }

  template <>
  std::string
  HDF5Object::attr(const std::string attr_name) const
  {
    // Reads a UTF8 python string
    //
    // code from
    // https://support.hdfgroup.org/ftp/HDF5/examples/misc-examples/vlstratt.c
    //
    // In the case of a variable length string the user does not have to reserve
    // memory for string_out. The call HAread will reserve the memory and the
    // user has to free the memory.
    //
    // Todo:
    // - In debug mode, check the type of the attribute. If it is not a variable
    // length string raise an exception
    // - Use valgrind to check that there are no memory leaks
    // - Use H5Dvlen_reclaim instead of free
    // - Use collective

    char * string_out;
    hid_t  attr;
    hid_t  type;
    herr_t ret;

    /* Create a datatype to refer to. */
    type = H5Tcopy(H5T_C_S1);
    Assert(type >= 0, ExcInternalError());

    // Python strings are encoded in UTF8
    ret = H5Tset_cset(type, H5T_CSET_UTF8);
    Assert(type >= 0, ExcInternalError());

    ret = H5Tset_size(type, H5T_VARIABLE);
    Assert(ret >= 0, ExcInternalError());

    attr = H5Aopen(*hdf5_reference, attr_name.data(), H5P_DEFAULT);
    Assert(attr >= 0, ExcInternalError());

    ret = H5Aread(attr, type, &string_out);
    Assert(ret >= 0, ExcInternalError());

    std::string string_value(string_out);
    // The memory of the variable length string has to be freed.
    // H5Dvlen_reclaim could be also used
    free(string_out);
    H5Tclose(type);
    H5Aclose(attr);
    return string_value;
  }

  template <>
  FullMatrix<double>
  HDF5Object::attr(const std::string attr_name) const
  {
    hid_t attr;
    hid_t attr_space;
    attr       = H5Aopen(*hdf5_reference, attr_name.data(), H5P_DEFAULT);
    attr_space = H5Aget_space(attr);
    AssertDimension(H5Sget_simple_extent_ndims(attr_space), 2);
    hsize_t dims[2];
    H5Sget_simple_extent_dims(attr_space, dims, NULL);

    FullMatrix<double> matrix_value(dims[0], dims[1]);

    double *hdf5_data = (double *)malloc(dims[0] * dims[1] * sizeof(double));
    H5Aread(attr, H5T_NATIVE_DOUBLE, hdf5_data);
    for (unsigned int dim0_idx = 0; dim0_idx < dims[0]; ++dim0_idx)
      {
        for (unsigned int dim1_idx = 0; dim1_idx < dims[1]; ++dim1_idx)
          {
            matrix_value[dim0_idx][dim1_idx] =
              hdf5_data[dim0_idx * dims[1] + dim1_idx];
          }
      }

    H5Sclose(attr_space);
    H5Aclose(attr);

    free(hdf5_data);

    return matrix_value;
  }

  template <typename T>
  void
  HDF5Object::write_attr(const std::string attr_name, T value) const
  {
    hid_t                  attr;
    hid_t                  aid;
    std::shared_ptr<hid_t> t_type = internal::get_hdf5_datatype<T>();


    /*
     * Create scalar attribute.
     */
    aid  = H5Screate(H5S_SCALAR);
    attr = H5Acreate2(*hdf5_reference,
                      attr_name.data(),
                      *t_type,
                      aid,
                      H5P_DEFAULT,
                      H5P_DEFAULT);

    /*
     * Write scalar attribute.
     */
    H5Awrite(attr, *t_type, &value);

    H5Sclose(aid);
    H5Aclose(attr);
  }

  template <typename T>
  DataSet<T>::DataSet(const std::string    name,
                      const hid_t &        parent_group_id,
                      std::vector<hsize_t> dimensions,
                      const bool           mpi,
                      const Mode           mode)
    : HDF5Object(name, mpi)
    , rank(dimensions.size())
    , dimensions(dimensions)
    , t_type(internal::get_hdf5_datatype<T>())
  {
    hdf5_reference = std::shared_ptr<hid_t>(new hid_t, [](auto pointer) {
      // Relase the HDF5 resource
      H5Dclose(*pointer);
      delete pointer;
    });
    dataspace      = std::shared_ptr<hid_t>(new hid_t, [](auto pointer) {
      // Relase the HDF5 resource
      H5Sclose(*pointer);
      delete pointer;
    });


    total_size = 1;
    for (auto &&dimension : dimensions)
      {
        total_size *= dimension;
      }

    switch (mode)
      {
        case (Mode::create):
          *dataspace = H5Screate_simple(rank, dimensions.data(), NULL);

          *hdf5_reference = H5Dcreate2(parent_group_id,
                                       name.data(),
                                       *t_type,
                                       *dataspace,
                                       H5P_DEFAULT,
                                       H5P_DEFAULT,
                                       H5P_DEFAULT);
          break;
        case (Mode::open):
          // Write the code for open
          *hdf5_reference = H5Gopen2(parent_group_id, name.data(), H5P_DEFAULT);
          break;
        default:
          Assert(false, ExcInternalError());
          break;
      }
  }

  template <typename T>
  DataSet<T>::~DataSet()
  {
    // Close first the dataset
    hdf5_reference.reset();
    // Then close the dataspace
    dataspace.reset();
  }

  template <typename T>
  void
  DataSet<T>::write_data(const std::vector<T> &data) const
  {
    AssertDimension(total_size, data.size());
    hid_t plist;

    if (mpi)
      {
        plist = H5Pcreate(H5P_DATASET_XFER);
        H5Pset_dxpl_mpio(plist, H5FD_MPIO_COLLECTIVE);
      }
    else
      {
        plist = H5P_DEFAULT;
      }

    H5Dwrite(*hdf5_reference, *t_type, H5S_ALL, H5S_ALL, plist, data.data());

    if (mpi)
      {
        H5Pclose(plist);
      }
  }

  template <typename T>
  void
  DataSet<T>::write_data(const FullMatrix<T> &data) const
  {
    AssertDimension(total_size, data.m() * data.n());
    hid_t plist;

    if (mpi)
      {
        plist = H5Pcreate(H5P_DATASET_XFER);
        H5Pset_dxpl_mpio(plist, H5FD_MPIO_COLLECTIVE);
      }
    else
      {
        plist = H5P_DEFAULT;
      }

    // The iterator of FullMatrix has to be converted to a pointer to the raw
    // data
    H5Dwrite(*hdf5_reference, *t_type, H5S_ALL, H5S_ALL, plist, &*data.begin());

    if (mpi)
      {
        H5Pclose(plist);
      }
  }

  template <typename T>
  void
  DataSet<T>::write_data_selection(const std::vector<T> &     data,
                                   const std::vector<hsize_t> coordinates) const
  {
    AssertDimension(coordinates.size(), data.size() * rank);
    hid_t                memory_dataspace;
    hid_t                plist;
    std::vector<hsize_t> data_dimensions = {data.size()};

    memory_dataspace = H5Screate_simple(1, data_dimensions.data(), NULL);
    H5Sselect_elements(*dataspace,
                       H5S_SELECT_SET,
                       data.size(),
                       coordinates.data());

    if (mpi)
      {
        plist = H5Pcreate(H5P_DATASET_XFER);
        H5Pset_dxpl_mpio(plist, H5FD_MPIO_COLLECTIVE);
      }
    else
      {
        plist = H5P_DEFAULT;
      }

    H5Dwrite(*hdf5_reference,
             *t_type,
             memory_dataspace,
             *dataspace,
             plist,
             data.data());

    if (mpi)
      {
        H5Pclose(plist);
      }
    H5Sclose(memory_dataspace);
  }

  template <typename T>
  void
  DataSet<T>::write_data_hyperslab(const std::vector<T> &     data,
                                   const std::vector<hsize_t> offset,
                                   const std::vector<hsize_t> count) const
  {
    AssertDimension(std::accumulate(count.begin(),
                                    count.end(),
                                    1,
                                    std::multiplies<unsigned int>()),
                    data.size());
    hid_t                memory_dataspace;
    hid_t                plist;
    std::vector<hsize_t> data_dimensions = {data.size()};

    memory_dataspace = H5Screate_simple(1, data_dimensions.data(), NULL);
    H5Sselect_hyperslab(
      *dataspace, H5S_SELECT_SET, offset.data(), NULL, count.data(), NULL);

    if (mpi)
      {
        plist = H5Pcreate(H5P_DATASET_XFER);
        H5Pset_dxpl_mpio(plist, H5FD_MPIO_COLLECTIVE);
      }
    else
      {
        plist = H5P_DEFAULT;
      }
    H5Dwrite(*hdf5_reference,
             *t_type,
             memory_dataspace,
             *dataspace,
             plist,
             &*data.begin());

    if (mpi)
      {
        H5Pclose(plist);
      }
    H5Sclose(memory_dataspace);
  }

  template <typename T>
  void
  DataSet<T>::write_data_hyperslab(const FullMatrix<T> &      data,
                                   const std::vector<hsize_t> offset,
                                   const std::vector<hsize_t> count) const
  {
    AssertDimension(std::accumulate(count.begin(),
                                    count.end(),
                                    1,
                                    std::multiplies<unsigned int>()),
                    data.m() * data.n());
    hid_t                memory_dataspace;
    hid_t                plist;
    std::vector<hsize_t> data_dimensions = {data.m(), data.n()};

    memory_dataspace = H5Screate_simple(2, data_dimensions.data(), NULL);
    H5Sselect_hyperslab(
      *dataspace, H5S_SELECT_SET, offset.data(), NULL, count.data(), NULL);

    if (mpi)
      {
        plist = H5Pcreate(H5P_DATASET_XFER);
        H5Pset_dxpl_mpio(plist, H5FD_MPIO_COLLECTIVE);
      }
    else
      {
        plist = H5P_DEFAULT;
      }
    H5Dwrite(*hdf5_reference,
             *t_type,
             memory_dataspace,
             *dataspace,
             plist,
             &*data.begin());

    if (mpi)
      {
        H5Pclose(plist);
      }
    H5Sclose(memory_dataspace);
  }

  template <typename T>
  void
  DataSet<T>::write_data_none() const
  {
    hid_t                memory_dataspace;
    hid_t                plist;
    std::vector<hsize_t> data_dimensions = {0};

    memory_dataspace = H5Screate_simple(1, data_dimensions.data(), NULL);
    H5Sselect_none(*dataspace);

    if (mpi)
      {
        plist = H5Pcreate(H5P_DATASET_XFER);
        H5Pset_dxpl_mpio(plist, H5FD_MPIO_COLLECTIVE);
      }
    else
      {
        plist = H5P_DEFAULT;
      }

    // The pointer of data can safely be NULL, see the discussion at the HDF5
    // forum:
    // https://forum.hdfgroup.org/t/parallel-i-o-does-not-support-filters-yet/884/17
    H5Dwrite(
      *hdf5_reference, *t_type, memory_dataspace, *dataspace, plist, NULL);

    if (mpi)
      {
        H5Pclose(plist);
      }
    H5Sclose(memory_dataspace);
  }

  Group::Group(const std::string name,
               const Group &     parentGroup,
               const bool        mpi,
               const Mode        mode)
    : HDF5Object(name, mpi)
  {
    hdf5_reference = std::shared_ptr<hid_t>(new hid_t, [](auto pointer) {
      // Relase the HDF5 resource
      H5Gclose(*pointer);
      delete pointer;
    });
    switch (mode)
      {
        case (Mode::create):
          *hdf5_reference = H5Gcreate2(*(parentGroup.hdf5_reference),
                                       name.data(),
                                       H5P_DEFAULT,
                                       H5P_DEFAULT,
                                       H5P_DEFAULT);
          break;
        case (Mode::open):
          *hdf5_reference =
            H5Gopen2(*(parentGroup.hdf5_reference), name.data(), H5P_DEFAULT);
          break;
        default:
          Assert(false, ExcInternalError());
          break;
      }
  }

  Group::Group(const std::string name, const bool mpi)
    : HDF5Object(name, mpi)
  {}

  Group
  Group::group(const std::string name)
  {
    return Group(name, *this, mpi, Mode::open);
  }

  Group
  Group::create_group(const std::string name)
  {
    return Group(name, *this, mpi, Mode::create);
  }

  template <typename T>
  DataSet<T>
  Group::create_dataset(const std::string          name,
                        const std::vector<hsize_t> dimensions) const
  {
    return DataSet<T>(
      name, *hdf5_reference, dimensions, mpi, DataSet<T>::Mode::create);
  }

  template <typename T>
  void
  Group::write_dataset(const std::string name, const std::vector<T> &data) const
  {
    std::vector<hsize_t> dimensions = {data.size()};
    auto                 dataset    = create_dataset<T>(name, dimensions);
    dataset.write_data(data);
  }

  template <typename T>
  void
  Group::write_dataset(const std::string name, const FullMatrix<T> &data) const
  {
    std::vector<hsize_t> dimensions = {data.m(), data.n()};
    auto                 dataset    = create_dataset<T>(name, dimensions);
    dataset.write_data(data);
  }


  File::File(const std::string name,
             const bool        mpi,
             const MPI_Comm    mpi_communicator,
             const Mode        mode)
    : Group(name, mpi)
  {
    hdf5_reference = std::shared_ptr<hid_t>(new hid_t, [](auto pointer) {
      // Relase the HDF5 resource
      H5Fclose(*pointer);
      delete pointer;
    });

    hid_t          plist;
    const MPI_Info info = MPI_INFO_NULL;

    if (mpi)
      {
        plist = H5Pcreate(H5P_FILE_ACCESS);
        H5Pset_fapl_mpio(plist, mpi_communicator, info);
      }
    else
      {
        plist = H5P_DEFAULT;
      }

    switch (mode)
      {
        case (Mode::create):
          *hdf5_reference =
            H5Fcreate(name.data(), H5F_ACC_TRUNC, H5P_DEFAULT, plist);
          break;
        case (Mode::open):
          *hdf5_reference = H5Fopen(name.data(), H5F_ACC_RDWR, plist);
          break;
        default:
          Assert(false, ExcInternalError());
          break;
      }

    if (mpi)
      {
        // Relase the HDF5 resource
        H5Pclose(plist);
      }
  }

  File::File(const std::string name,
             const MPI_Comm    mpi_communicator,
             const Mode        mode)
    : File(name, true, mpi_communicator, mode)
  {}

  File::File(const std::string name, const Mode mode)
    : File(name, false, MPI_COMM_NULL, mode)
  {}


  // explicit instantiations of classes
  template class DataSet<float>;
  template class DataSet<double>;
  template class DataSet<long double>;
  template class DataSet<std::complex<float>>;
  template class DataSet<std::complex<double>>;
  template class DataSet<std::complex<long double>>;
  template class DataSet<int>;
  template class DataSet<unsigned int>;


  // explicit instantiations of functions
  template float
  HDF5Object::attr<float>(const std::string attr_name) const;
  template double
  HDF5Object::attr<double>(const std::string attr_name) const;
  template long double
  HDF5Object::attr<long double>(const std::string attr_name) const;
  template std::complex<float>
  HDF5Object::attr<std::complex<float>>(const std::string attr_name) const;
  template std::complex<double>
  HDF5Object::attr<std::complex<double>>(const std::string attr_name) const;
  template std::complex<long double>
  HDF5Object::attr<std::complex<long double>>(
    const std::string attr_name) const;
  template int
  HDF5Object::attr<int>(const std::string attr_name) const;
  template unsigned int
  HDF5Object::attr<unsigned int>(const std::string attr_name) const;
  // The specialization of HDF5Object::attr<std::string> has been defined above

  template void
  HDF5Object::write_attr<float>(const std::string attr_name, float value) const;
  template void
  HDF5Object::write_attr<double>(const std::string attr_name,
                                 double            value) const;
  template void
  HDF5Object::write_attr<long double>(const std::string attr_name,
                                      long double       value) const;
  template void
  HDF5Object::write_attr<std::complex<float>>(const std::string   attr_name,
                                              std::complex<float> value) const;
  template void
  HDF5Object::write_attr<std::complex<double>>(
    const std::string    attr_name,
    std::complex<double> value) const;
  template void
  HDF5Object::write_attr<std::complex<long double>>(
    const std::string         attr_name,
    std::complex<long double> value) const;
  template void
  HDF5Object::write_attr<int>(const std::string attr_name, int value) const;
  template void
  HDF5Object::write_attr<unsigned int>(const std::string attr_name,
                                       unsigned int      value) const;


  template DataSet<float>
  Group::create_dataset<float>(const std::string          name,
                               const std::vector<hsize_t> dimensions) const;
  template DataSet<double>
  Group::create_dataset<double>(const std::string          name,
                                const std::vector<hsize_t> dimensions) const;
  template DataSet<long double>
  Group::create_dataset<long double>(
    const std::string          name,
    const std::vector<hsize_t> dimensions) const;
  template DataSet<std::complex<float>>
  Group::create_dataset<std::complex<float>>(
    const std::string          name,
    const std::vector<hsize_t> dimensions) const;
  template DataSet<std::complex<double>>
  Group::create_dataset<std::complex<double>>(
    const std::string          name,
    const std::vector<hsize_t> dimensions) const;
  template DataSet<std::complex<long double>>
  Group::create_dataset<std::complex<long double>>(
    const std::string          name,
    const std::vector<hsize_t> dimensions) const;
  template DataSet<int>
  Group::create_dataset<int>(const std::string          name,
                             const std::vector<hsize_t> dimensions) const;
  template DataSet<unsigned int>
  Group::create_dataset<unsigned int>(
    const std::string          name,
    const std::vector<hsize_t> dimensions) const;

  template void
  Group::write_dataset(const std::string         name,
                       const std::vector<float> &data) const;
  template void
  Group::write_dataset(const std::string          name,
                       const std::vector<double> &data) const;
  template void
  Group::write_dataset(const std::string               name,
                       const std::vector<long double> &data) const;
  template void
  Group::write_dataset(const std::string                       name,
                       const std::vector<std::complex<float>> &data) const;
  template void
  Group::write_dataset(const std::string                        name,
                       const std::vector<std::complex<double>> &data) const;
  template void
  Group::write_dataset(
    const std::string                             name,
    const std::vector<std::complex<long double>> &data) const;
  template void
  Group::write_dataset(const std::string       name,
                       const std::vector<int> &data) const;
  template void
  Group::write_dataset(const std::string                name,
                       const std::vector<unsigned int> &data) const;

  template void
  Group::write_dataset(const std::string        name,
                       const FullMatrix<float> &data) const;
  template void
  Group::write_dataset(const std::string         name,
                       const FullMatrix<double> &data) const;
  template void
  Group::write_dataset(const std::string              name,
                       const FullMatrix<long double> &data) const;
  template void
  Group::write_dataset(const std::string                      name,
                       const FullMatrix<std::complex<float>> &data) const;
  template void
  Group::write_dataset(const std::string                       name,
                       const FullMatrix<std::complex<double>> &data) const;
  template void
  Group::write_dataset(const std::string                            name,
                       const FullMatrix<std::complex<long double>> &data) const;
} // namespace HDF5

DEAL_II_NAMESPACE_CLOSE
