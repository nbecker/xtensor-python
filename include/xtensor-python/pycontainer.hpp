/***************************************************************************
* Copyright (c) 2016, Johan Mabille and Sylvain Corlay                     *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#ifndef PY_CONTAINER_HPP
#define PY_CONTAINER_HPP

#include <functional>
#include <numeric>
#include <cmath>

#include "pybind11/pybind11.h"
#include "pybind11/common.h"
#include "pybind11/complex.h"

#ifndef FORCE_IMPORT_ARRAY
#define NO_IMPORT_ARRAY
#endif
#ifndef PY_ARRAY_UNIQUE_SYMBOL
#define PY_ARRAY_UNIQUE_SYMBOL xtensor_python_ARRAY_API
#endif
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION

#include "numpy/arrayobject.h"

#include "xtensor/xcontainer.hpp"

namespace xt
{

    inline void import_numpy();

    /**
     * @class pycontainer
     * @brief Base class for xtensor containers wrapping numpy arryays.
     *
     * The pycontainer class should not be instantiated directly. Instead, used should
     * use pytensor and pyarray instancs.
     *
     * @tparam D The derived type, i.e. the inheriting class for which pycontainer
     *           provides the interface.
     */
    template <class D>
    class pycontainer : public pybind11::object,
                        public xcontainer<D>
    {
    public:

        using derived_type = D;

        using base_type = xcontainer<D>;
        using inner_types = xcontainer_inner_types<D>;
        using container_type = typename inner_types::container_type;
        using value_type = typename container_type::value_type;
        using reference = typename container_type::reference;
        using const_reference = typename container_type::const_reference;
        using pointer = typename container_type::pointer;
        using const_pointer = typename container_type::const_pointer;
        using size_type = typename container_type::size_type;
        using difference_type = typename container_type::difference_type;

        using shape_type = typename inner_types::shape_type;
        using strides_type = typename inner_types::strides_type;
        using backstrides_type = typename inner_types::backstrides_type;
        using inner_shape_type = typename inner_types::inner_shape_type;
        using inner_strides_type = typename inner_types::inner_strides_type;

        using iterable_base = xiterable<D>;

        using iterator = typename iterable_base::iterator;
        using const_iterator = typename iterable_base::const_iterator;

        using stepper = typename iterable_base::stepper;
        using const_stepper = typename iterable_base::const_stepper;

        using broadcast_iterator = typename iterable_base::broadcast_iterator;
        using const_broadcast_iterator = typename iterable_base::broadcast_iterator;

        void reshape(const shape_type& shape);
        void reshape(const shape_type& shape, layout l);
        void reshape(const shape_type& shape, const strides_type& strides);

        using base_type::operator();
        using base_type::operator[];
        using base_type::begin;
        using base_type::end;

    protected:

        pycontainer();
        ~pycontainer() = default;

        pycontainer(pybind11::handle h, borrowed_t);
        pycontainer(pybind11::handle h, stolen_t);
        pycontainer(const pybind11::object& o);

        pycontainer(const pycontainer&) = default;
        pycontainer& operator=(const pycontainer&) = default;

        pycontainer(pycontainer&&) = default;
        pycontainer& operator=(pycontainer&&) = default;

        static derived_type ensure(pybind11::handle h);
        static bool check_(pybind11::handle h);
        static PyObject* raw_array_t(PyObject* ptr);

        PyArrayObject* python_array();
    };

    namespace detail
    {
        template <class T>
        struct numpy_traits
        {
        private:

            constexpr static const int value_list[15] = {
                NPY_BOOL,
                NPY_BYTE,   NPY_UBYTE,   NPY_SHORT,      NPY_USHORT,
                NPY_INT,    NPY_UINT,    NPY_LONGLONG,   NPY_ULONGLONG,
                NPY_FLOAT,  NPY_DOUBLE,  NPY_LONGDOUBLE,
                NPY_CFLOAT, NPY_CDOUBLE, NPY_CLONGDOUBLE
            };

        public:

            using value_type = std::remove_const_t<T>;

            static constexpr int type_num = value_list[pybind11::detail::is_fmt_numeric<value_type>::index];
        };
    }

    /******************************
     * pycontainer implementation *
     ******************************/

    template <class D>
    inline pycontainer<D>::pycontainer()
        : pybind11::object()
    {
    }

    template <class D>
    inline pycontainer<D>::pycontainer(pybind11::handle h, borrowed_t b)
        : pybind11::object(h, b)
    {
    }

    template <class D>
    inline pycontainer<D>::pycontainer(pybind11::handle h, stolen_t s)
        : pybind11::object(h, s)
    {
    }
    
    template <class D>
    inline pycontainer<D>::pycontainer(const pybind11::object& o)
        : pybind11::object(raw_array_t(o.ptr()), pybind11::object::stolen)
    {
        if (!this->m_ptr)
        {
            throw pybind11::error_already_set();
        }
    }

    template <class D>
    inline auto pycontainer<D>::ensure(pybind11::handle h) -> derived_type
    {
        auto result = pybind11::reinterpret_steal<derived_type>(raw_array_t(h.ptr()));
        if (result.ptr() == nullptr)
        {
            PyErr_Clear();
        }
        return result;
    }

    template <class D>
    inline bool pycontainer<D>::check_(pybind11::handle h)
    {
        int type_num = detail::numpy_traits<value_type>::type_num;
        return PyArray_Check(h.ptr()) &&
            PyArray_EquivTypenums(PyArray_TYPE(reinterpret_cast<PyArrayObject*>(h.ptr())), type_num);
    }

    template <class D>
    inline PyObject* pycontainer<D>::raw_array_t(PyObject* ptr)
    {
        if (ptr == nullptr)
        {
            return nullptr;
        }
        int type_num = detail::numpy_traits<value_type>::type_num;
        auto res = PyArray_FromAny(ptr, PyArray_DescrFromType(type_num), 0, 0, 
                                   NPY_ARRAY_ENSUREARRAY | NPY_ARRAY_FORCECAST, nullptr);
        return res;
    }

    template <class D>
    inline PyArrayObject* pycontainer<D>::python_array()
    {
        return reinterpret_cast<PyArrayObject*>(this->m_ptr);
    }

    /**
     * Reshapes the container.
     * @param shape the new shape
     */
    template <class D>
    inline void pycontainer<D>::reshape(const shape_type& shape)
    {
        if (shape.size() != this->dimension() || !std::equal(shape.begin(), shape.end(), this->shape().begin()))
        {
            reshape(shape, layout::row_major);
        }
    }

    /**
     * Reshapes the container.
     * @param shape the new shape
     * @param l the new layout
     */
    template <class D>
    inline void pycontainer<D>::reshape(const shape_type& shape, layout l)
    {
        strides_type strides = make_sequence<strides_type>(shape.size(), size_type(1));
        compute_strides(shape, l, strides);
        reshape(shape, strides);
    }

    /**
     * Reshapes the container.
     * @param shape the new shape
     * @param strides the new strides
     */
    template <class D>
    inline void pycontainer<D>::reshape(const shape_type& shape, const strides_type& strides)
    {
        derived_type tmp(shape, strides);
        *static_cast<derived_type*>(this) = std::move(tmp);
    }

    /**
     * Import the numpy Python module.
     */
    inline void import_numpy()
    {
        #ifdef FORCE_IMPORT_ARRAY
        if (_import_array() < 0)
        {
            PyErr_Print();
            PyErr_SetString(PyExc_ImportError, "numpy.core.multiarray failed to import");
        }
        #endif
    }
}

#endif

