/*      This program is free software; you can redistribute it and/or modify
*      it under the terms of the GNU General Public License as published by
*      the Free Software Foundation; either version 3 of the License, or
*      (at your option) any later version.
*
*      This program is distributed in the hope that it will be useful,
*      but WITHOUT ANY WARRANTY; without even the implied warranty of
*      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*      GNU General Public License for more details.
*
*      You should have received a copy of the GNU General Public License
*      along with this program; if not, write to the Free Software
*      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
*      MA 02110-1301, USA.
*
*      Author:
*      John Joseph
*
*/

#pragma once

#include <memory>

#include <Python.h>
#include <structmember.h>

#include "pyl_convert.h"

namespace pyl
{
	// Every python module function looks like this
	using PyFunc = std::function<PyObject *(PyObject *, PyObject *)>;

	// Deleter that calls Py_XDECREF on the PyObject parameter.
	struct PyObjectDeleter {
		void operator()(PyObject *obj) {
			Py_XDECREF(obj);
		}
	};
	// unique_ptr that uses Py_XDECREF as the destructor function.
	using pyunique_ptr = std::unique_ptr<PyObject, PyObjectDeleter> ;

	// All exposed objects inherit from this python type, which has a capsule
	// member holding a pointer to the original object
	struct GenericPyClass
	{
        PyObject_HEAD
		PyObject * capsule{ nullptr };
	};

	// Several python APIs require a null terminated array of data
	// So that's what this class does (every insertion goes before a null element)
	// Because this uses a std::vector under the hood inserting will move things in memory,
	// which is why it's important not to modify any of these structs after they've been 
	// expsoed to the python interpreter
	template <typename D>
	struct _NullTermBuf
	{
		std::vector<D> v_Data;
		D * Ptr() { return v_Data.data(); }
		_NullTermBuf() :v_Data( 1, { 0 } ) {}
	protected:
		// Insert elements before the last (null) element
		void _insert(D data) { v_Data.insert(v_Data.end() - 1, data); }
	};

	// Null terminated method defs
	struct MethodDefinitions : public _NullTermBuf<PyMethodDef>
	{
		// Just use the base
		MethodDefinitions() : _NullTermBuf() {}
		
		// These containers don't invalidate references
		std::list<std::string> MethodNames, MethodDocs;

		// Add method definitions before the null terminator
		void AddMethod(std::string name, PyCFunction fnPtr, int flags, std::string docs = "");
	};

	// Null terminated member defs
	struct MemberDefinitions : public _NullTermBuf<PyMemberDef>
	{
		// These containers don't invalidate references
		std::list<std::string> MemberNames, MemberDocs;

		// Use base
		MemberDefinitions();// : NullTermBuf() {}

		// Add method definitions before the null terminator
		void AddMember(std::string name, int type, int offset, int flags, std::string docs = "");
	};

	// Defines an exposed class (which is not per instance)
	// as well as a list of exposed instances
	struct ExposedClass
	{
		// The name of the python class
		std::string PyClassName;
        
		// Each exposed class has a method definition
		MethodDefinitions m_MethodDef;
		
        // And members
		MemberDefinitions m_MemberDef;

		// The Python type object
		PyTypeObject m_TypeObject;

		// Lock down pointers
		void Prepare();

		// Ref to type object
		PyTypeObject& to() { return m_TypeObject; }

		// Add method definitions before the null terminator
		void AddMemberFn(std::string name, PyCFunction fnPtr, int flags, std::string docs = "") {
			m_MethodDef.AddMethod(name, fnPtr, flags, docs);
		}
        
        // Add member definitions (which isn't really a thing we want to do...)
		void AddMember(std::string name, int type, int offset, int flags, std::string doc = "") {
			m_MemberDef.AddMember(name, type, offset, flags, doc);
		}

        // Default constructor
        ExposedClass(std::string n = "unnamed");
	};

	// TODO more doxygen!
	// This is the original pywrapper::object... quite the beast
	/**
	* \class Object
	* \brief This class represents a python object.
	*/
	class Object {
	public:
		/**
		* \brief Constructs a default python object
		*/
		Object();

		/**
		* \brief Constructs a python object from a PyObject pointer.
		*
		* This Object takes ownership of the PyObject* argument. That
		* means no Py_INCREF is performed on it.
		* \param obj The pointer from which to construct this Object.
		*/
		Object(PyObject *obj);

		/**
		* \brief Calls the callable attribute "name" using the provided
		* arguments.
		*
		* This function might throw a std::runtime_error if there is
		* an error when calling the function.
		*
		* \param name The name of the attribute to be called.
		* \param args The arguments which will be used when calling the
		* attribute.
		* \return pyl::Object containing the result of the function.
		*/
		template<typename... Args>
		Object call_function(const std::string &name, const Args&... args) {
			pyunique_ptr func(load_function(name));
			// Create the tuple argument
			pyunique_ptr tup(PyTuple_New(sizeof...(args)));
			add_tuple_vars(tup, args...);

			// Call our object
			PyObject *ret(PyObject_CallObject(func.get(), tup.get()));
			if ( !ret )
			{
				print_error();
				throw std::runtime_error( "Failed to call function " + name );
			}
			return{ ret };
		}

		/**
		* \brief Calls a callable attribute using no arguments.
		*
		* This function might throw a std::runtime_error if there is
		* an error when calling the function.
		*
		* \sa pyl::Object::call_function.
		* \param name The name of the callable attribute to be executed.
		* \return pyl::Object containing the result of the function.
		*/
		Object call_function(const std::string &name);

		/**
		* \brief Finds and returns the attribute named "name".
		*
		* This function might throw a std::runtime_error if an error
		* is encountered while fetching the attribute.
		*
		* \param name The name of the attribute to be returned.
		* \return pyl::Object representing the attribute.
		*/
		Object get_attr(const std::string &name);

		/**
		* \brief Checks whether this object contains a certain attribute.
		*
		* \param name The name of the attribute to be searched.
		* \return bool indicating whether the attribute is defined.
		*/
		bool has_attr(const std::string &name);
        
        template<typename T>
        bool set_attr(const std::string &name, T obj){
            PyObject * pyObj = alloc_pyobject(obj);
            int success = PyObject_SetAttrString(this->get(), name.c_str(), pyObj);
            return (success == 0);
        }

		/**
		* \brief Returns the internal PyObject*.
		*
		* No reference increment is performed on the PyObject* before
		* returning it, so any DECREF applied to it without INCREF'ing
		* it will cause undefined behaviour.
		* \return The PyObject* which this Object is representing.
		*/
		PyObject *get() const { return py_obj.get(); }

		template<class T>
		bool convert(T &param) {
			return pyl::convert(py_obj.get(), param);
		}

		/**
		* \brief Constructs a pyl::Object from a script.
		*
		* The returned Object will be the representation of the loaded
		* script. If any errors are encountered while loading this
		* script, a std::runtime_error is thrown.
		*
		* \param script_path The path of the script to be loaded.
		* \return Object representing the loaded script.
		*/
		static Object from_script(const std::string &script_path);

	protected:
		typedef std::shared_ptr<PyObject> pyshared_ptr;

		PyObject *load_function(const std::string &name);

		pyshared_ptr make_pyshared(PyObject *obj);

		// Variadic template method to add items to a tuple
		template<typename First, typename... Rest>
		void add_tuple_vars(pyunique_ptr &tup, const First &head, const Rest&... tail) {
			add_tuple_var(
				tup,
				PyTuple_Size(tup.get()) - sizeof...(tail)-1,
				head
				);
			add_tuple_vars(tup, tail...);
		}


		void add_tuple_vars(pyunique_ptr &tup, PyObject *arg) {
			add_tuple_var(tup, PyTuple_Size(tup.get()) - 1, arg);
		}

		// Base case for add_tuple_vars
		template<typename Arg>
		void add_tuple_vars(pyunique_ptr &tup, const Arg &arg) {
			add_tuple_var(tup,
				PyTuple_Size(tup.get()) - 1, alloc_pyobject(arg)
				);
		}

		// Adds a PyObject* to the tuple object
		void add_tuple_var(pyunique_ptr &tup, Py_ssize_t i, PyObject *pobj) {
			PyTuple_SetItem(tup.get(), i, pobj);
		}

		// Adds a PyObject* to the tuple object
		template<class T> void add_tuple_var(pyunique_ptr &tup, Py_ssize_t i,
			const T &data) {
			PyTuple_SetItem(tup.get(), i, alloc_pyobject(data));
		}

		pyshared_ptr py_obj;
	};
}
