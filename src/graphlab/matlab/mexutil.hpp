#ifndef MEXUTIL_HPP
#define MEXUTIL_HPP
#include <iostream>
#include <typeinfo>
#include <cstring>
#include "mex_classid_traits.hpp"

/** Gets the result of mxGetElementSize(..) on an array created using the classID */
inline size_t get_class_element_size(mxClassID cid) {
  switch(cid) {
    case mxUNKNOWN_CLASS:
    case mxCELL_CLASS:
    case mxSTRUCT_CLASS:
    case mxLOGICAL_CLASS:
    case mxVOID_CLASS:
    case mxFUNCTION_CLASS:
      return 0;
    case mxINT8_CLASS:
    case mxUINT8_CLASS:
      return 1;
    case mxCHAR_CLASS:  // yes char is here. matlab uses wide chars
    case mxINT16_CLASS:
    case mxUINT16_CLASS:
      return 2;
    case mxINT32_CLASS:
    case mxUINT32_CLASS:
      return 4;
    case mxDOUBLE_CLASS:
    case mxINT64_CLASS:
    case mxUINT64_CLASS:
      return 8;
  }
}


template<class T, class EMXType>
bool clear_array(EMXType &out) {
  out.data = (T*)malloc(sizeof(T) * 1);
  out.size = (int32_t*)malloc(sizeof(int32_t) * 2);
  out.size[0] = 1; out.size[1] = 1;
  out.allocatedSize = 1;
  out.numDimensions = 2;
  out.canFreeData = 1;
  return true;
}


/**
 * Converts an array to an emxarray assuming the internal storage of array
 * is storage compatible
 */
template<class T, class EMXType>
bool read_storage_compatible_array(const mxArray *array, EMXType &out) {
  bool printed = false;
  // get the number of dimensions
  out.numDimensions = mxGetNumberOfDimensions(array);
  // get the dimensions. Allocate the size array and copy it
  // we have to do this element by element since mwSize may not be the same
  // type as int32_t
  const mwSize* dimptr = mxGetDimensions(array);
  out.size = (int32_t *)malloc((sizeof(int32_t) * out.numDimensions));
  // count the number of elements
  size_t numel = 1;
  for (size_t i = 0; i < out.numDimensions; ++i) {
    out.size[i] = dimptr[i];
    numel = numel * dimptr[i];
  }
  
  // fill in the allocated size and allocate the data
  out.allocatedSize = numel;
  out.data = (T*)malloc((sizeof(T) * numel));
  out.canFreeData = 1;
  // copy the data. Since we have guaranteed type storage compatibility, 
  // we can do a direct memcpy
  void* ptr = NULL;
  ptr = mxGetData(array);
  memcpy(out.data, ptr, sizeof(T) * numel);
  return true;
}

/**
 * Converts a matlab character string into a emxArray
 */
template<class T, class EMXType>
bool read_byte_array(const mxArray *array, EMXType &out) {
  static bool printed = false;
  // character matrix not supported
  if (mxGetN(array) != 1 &&  mxGetM(array) != 1) {
    if (!printed) std::cerr << "char matrix not supported" << std::endl;
    printed = true;
    return false;
  }
  size_t numel = mxGetN(array) * mxGetM(array);
  // allocate a string
  out.data = (T*)malloc(numel+1);
  out.allocatedSize = numel + 1;
  out.numDimensions = 2;
  out.size = (int32_t *)malloc(2 * sizeof(int32_t));
  out.size[0] = numel; out.size[1] = 1;
  out.canFreeData = 1;
  memset(out.data, 0, numel+1);
  
  //matlab stores as wide chars. compact to regular chars
  char ptr[numel + 1];
  // read the string
  mxGetString(array, ptr, numel + 1);
  memcpy(out.data, ptr, numel);
  return true;
}

/**
 * Converts an mxArray to an emxArray(EMXType ) of a particular type (T)
 * out MUST be an unallocated emxArray structure. 
 * Returns true on success, false on failure
 */
template<class T, class EMXType>
bool read_array(const mxArray *array, EMXType &out) {
  // reset out
  static bool printed = false;
  if (array == NULL) {
    clear_array<T, EMXType>(out);
    return true;
  }
  // check for type compatibility
  // element sizes must be the same
  // and it must pass the class compatibility check
  if (compatible_classid<T>(mxGetClassID(array)) == false) {
    if (!printed) {
      std::cerr << "Parse type incompatibility when reading mxArray." << std::endl;
      std::cerr << "Failed conversion from " << mxGetClassName(array) << " to " << typeid(T).name() << std::endl;
      if (mxGetElementSize(array) != sizeof(T)) {
        std::cerr << mxGetElementSize(array) << " != " << sizeof(T) << std::endl;
      }
    }
    printed = true;
    clear_array<T, EMXType>(out);
    return false;
  }
  
  /*
   * Copy the data. Some types have some special conversion rules.
   */
  if (mxIsComplex(array)) {
    //ptr = mxGetImagData(array);
    if (!printed) std::cerr << "Complex types not supported. " << std::endl;
    printed = true;
    clear_array<T, EMXType>(out);
    return false;
  }
  else if (mxGetElementSize(array) == sizeof(T)){
    return read_storage_compatible_array<T, EMXType>(array, out);
  }
  else if (mxGetClassID(array) == mxCHAR_CLASS && sizeof(T) == 1) {
    return read_byte_array<T, EMXType>(array, out);
  }
  else {
    if (!printed) {
      std::cerr << "Parse type incompatibility when reading mxArray." << std::endl;
      std::cerr << "Failed conversion from " << mxGetClassName(array) << " to " << typeid(T).name() << std::endl;
      if (mxGetElementSize(array) != sizeof(T)) {
        std::cerr << mxGetElementSize(array) << " != " << sizeof(T) << std::endl;
      }
    }
    printed = true;
    clear_array<T, EMXType>(out);
    return false;
  }
}


/**
 * Converts an emxarray to an mxarray assuming the internal storage of array
 * is storage compatible
 */
template<class T, class EMXType>
bool write_storage_compatible_array(EMXType &in, mxArray * &array) {
  // get the dimensions. 
  mwSize dimptr[in.numDimensions];
  for (size_t i = 0; i < in.numDimensions; ++i) dimptr[i] = in.size[i];
  
  // allocate the array
  array = mxCreateNumericArray(in.numDimensions, 
                               dimptr, 
                               prefered_classid<T>::cid(), 
                               prefered_classid<T>::complex()?mxCOMPLEX:mxREAL);
   
  // copy the data. Since we have guaranteed type storage compatibility, 
  // we can do a direct memcpy
  void* ptr = NULL;
  ptr = mxGetData(array);
  memcpy(ptr, in.data, sizeof(T) * mxGetNumberOfElements(array));
}




/**
 * Converts an emxArray(EMXType ) of a particular type T to an mxArray
 * array must not be allocated.
 * Returns true on success, false on failure
 */
template<class T, class EMXType>
bool write_array(EMXType &in, mxArray * &array) {
  static bool printed = false;
  // empty array
  array = NULL;
  if (in.numDimensions == 0) {
    return true;
  }
  // get the desired class ID
  mxClassID cid = prefered_classid<T>::cid();
  // copy the data. Some types have special conversion rules
  if (prefered_classid<T>::complex()) {
    // TODO: this is wrong
    //ptr = mxGetImagData(array);
    if (!printed) std::cerr << "Complex types not supported. " << std::endl;
    printed = true;
    return false;
  }
  else if (get_class_element_size(cid) == sizeof(T)) {
    write_storage_compatible_array<T, EMXType>(in, array);
  }
  else if (cid == mxCHAR_CLASS && sizeof(T) == 1) {
    // it may not always be NULL terminated
    size_t len = 1;
    for (size_t i = 0;i < in.numDimensions; ++i) len *= in.size[i];
    char c[len+1];
    memcpy(c, in.data, len);
    c[len] = 0;
    array = mxCreateString((char*)(c));
  }
  else {
    if (!printed) {
      std::cerr << "Failed conversion from " << typeid(T).name() << " to " << mxGetClassName(array) << std::endl;
      std::cerr << "Bad! Notify the developers!" << std::endl;
    }
    printed = true;
    return false;
  }

  return true;
}



/**
 * Copy an emxArray(EMXType).
 * Returns true on success, false on failure
 */
template<class T, class EMXType>
bool copy_array(EMXType &dest, const EMXType &src) {
  dest.data = (T*)realloc(dest.data, sizeof(T) * src.allocatedSize);
  dest.size = (int32_t*)realloc(dest.size, sizeof(int32_t) * src.numDimensions);
  dest.allocatedSize = src.allocatedSize;
  dest.numDimensions = src.numDimensions;
  dest.canFreeData = 1;
  // copy
  for (size_t i = 0;i < src.numDimensions; ++i) dest.size[i] = src.size[i];
  memcpy(dest.data, src.data, src.allocatedSize * sizeof(T));
}


/**
 * If the mxArray is a struct and has a field, this is set to the field's mxArray
 * Returns NULL otherwise
 */
inline const mxArray* struct_has_field(const mxArray *array, const char *fieldname) {
  if (mxGetClassID(array) == mxSTRUCT_CLASS) {
      return mxGetField(array,0,fieldname);
  }
  return NULL;
}
#endif