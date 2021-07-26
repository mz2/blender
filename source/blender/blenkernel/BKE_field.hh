/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

/** \file
 * \ingroup bke
 */

#include <atomic>

#include "BLI_function_ref.hh"
#include "BLI_map.hh"
#include "BLI_optional_ptr.hh"
#include "BLI_user_counter.hh"
#include "BLI_vector.hh"
#include "BLI_virtual_array.hh"

#include "FN_cpp_type.hh"
#include "FN_multi_function.hh"

namespace blender::bke {

using fn::CPPType;
using fn::GMutableSpan;
using fn::GVArray;
using fn::GVArrayPtr;
using fn::MultiFunction;

class FieldInputKey {
 public:
  virtual ~FieldInputKey() = default;
  virtual uint64_t hash() const = 0;
  virtual const CPPType &type() const = 0;

  friend bool operator==(const FieldInputKey &a, const FieldInputKey &b)
  {
    return a.is_same_as(b);
  }

 private:
  virtual bool is_same_as(const FieldInputKey &other) const
  {
    UNUSED_VARS(other);
    return false;
  }
};

class FieldInputValue {
 public:
  virtual ~FieldInputValue() = default;
};

class AttributeFieldInputKey : public FieldInputKey {
 private:
  std::string name_;
  const CPPType *type_;

 public:
  AttributeFieldInputKey(std::string name, const CPPType &type)
      : name_(std::move(name)), type_(&type)
  {
  }

  uint64_t hash() const override
  {
    return get_default_hash_2(name_, type_);
  }

  const CPPType &type() const override
  {
    return *type_;
  }

  StringRefNull name() const
  {
    return name_;
  }

 private:
  bool is_same_as(const FieldInputKey &other) const
  {
    if (const AttributeFieldInputKey *other_typed = dynamic_cast<const AttributeFieldInputKey *>(
            &other)) {
      return other_typed->type_ == type_ && other_typed->name_ == name_;
    }
    return false;
  }
};

class GVArrayFieldInputValue : public FieldInputValue {
 private:
  optional_ptr<GVArray> varray_;

 public:
  GVArrayFieldInputValue(optional_ptr<GVArray> varray) : varray_(std::move(varray))
  {
  }

  const GVArray &varray() const
  {
    return *varray_;
  }
};

class FieldInputs {
 private:
  using InputMap = Map<std::reference_wrapper<const FieldInputKey>, const FieldInputValue *>;
  InputMap inputs_;

  friend class GField;

 public:
  InputMap::KeyIterator begin() const
  {
    return inputs_.keys().begin();
  }

  InputMap::KeyIterator end() const
  {
    return inputs_.keys().end();
  }

  void set_input(const FieldInputKey &key, const FieldInputValue &value)
  {
    *inputs_.lookup_ptr(key) = &value;
  }

  const FieldInputValue *get(const FieldInputKey &key) const
  {
    return inputs_.lookup_default(key, nullptr);
  }

  template<typename ValueT> const ValueT *get(const FieldInputKey &key) const
  {
    return dynamic_cast<const ValueT *>(this->get(key));
  }
};

template<typename T> class FieldOutput {
 private:
  optional_ptr<const VArray<T>> varray_;

 public:
  FieldOutput(optional_ptr<const VArray<T>> varray) : varray_(std::move(varray))
  {
  }

  optional_ptr<const VArray<T>> extract()
  {
    return std::move(varray_);
  }

  const VArray<T> &varray() const
  {
    return *varray_;
  }
};

class GFieldOutput {
 private:
  optional_ptr<const GVArray> varray_;

 public:
  GFieldOutput(optional_ptr<const GVArray> varray) : varray_(std::move(varray))
  {
  }

  const GVArray &varray_ref() const
  {
    return *varray_;
  }
};

class GField {
 private:
  mutable std::atomic<int> users_ = 1;

 public:
  virtual ~GField() = default;

  FieldInputs prepare_inputs() const
  {
    FieldInputs inputs;
    this->foreach_input_key([&](const FieldInputKey &key) { inputs.inputs_.add(key, nullptr); });
    return inputs;
  }

  virtual void foreach_input_key(FunctionRef<void(const FieldInputKey &key)> callback) const
  {
    UNUSED_VARS(callback);
  }

  virtual const CPPType &output_type() const = 0;

  virtual GFieldOutput evaluate_generic(IndexMask mask, const FieldInputs &inputs) const = 0;

  void user_add() const
  {
    users_.fetch_add(1);
  }

  void user_remove() const
  {
    const int new_users = users_.fetch_sub(1) - 1;
    if (new_users == 0) {
      delete this;
    }
  }
};

template<typename T> class Field : public GField {
 public:
  virtual FieldOutput<T> evaluate(IndexMask mask, const FieldInputs &inputs) const = 0;

  GFieldOutput evaluate_generic(IndexMask mask, const FieldInputs &inputs) const override
  {
    FieldOutput<T> output = this->evaluate(mask, inputs);
    return optional_ptr<const GVArray>{
        std::make_unique<fn::GVArray_For_VArray<T>>(output.extract().extract_owned())};
  }

  const CPPType &output_type() const override
  {
    return CPPType::get<T>();
  }
};

template<typename T> class ConstantField : public Field<T> {
 private:
  T value_;

 public:
  ConstantField(T value) : value_(std::move(value))
  {
  }

  FieldOutput<T> evaluate(IndexMask mask, const FieldInputs &UNUSED(inputs)) const
  {
    return optional_ptr<const VArray<T>>{
        std::make_unique<VArray_For_Single<T>>(value_, mask.min_array_size())};
  }
};

template<typename KeyT> class VArrayInputField : public GField {
 private:
  KeyT key_;

 public:
  template<typename... Args> VArrayInputField(Args &&... args) : key_(std::forward<Args>(args)...)
  {
  }

  void foreach_input_key(FunctionRef<void(const FieldInputKey &key)> callback) const override
  {
    callback(key_);
  }

  const CPPType &output_type() const override
  {
    return key_.type();
  }

  GFieldOutput evaluate_generic(IndexMask mask, const FieldInputs &inputs) const override
  {
    const GVArrayFieldInputValue *input = inputs.get<GVArrayFieldInputValue>(key_);
    if (input == nullptr) {
      return GFieldOutput{
          optional_ptr<const GVArray>{std::make_unique<fn::GVArray_For_SingleValueRef>(
              key_.type(), mask.min_array_size(), key_.type().default_value())}};
    }
    return GFieldOutput{optional_ptr<const GVArray>{input->varray()}};
  }
};

template<typename T, typename VArrayT> class VArrayField : public Field<T> {
 private:
  T default_value_;
  VArrayT varray_;

 public:
  template<typename... Args>
  VArrayField(T default_value, Args &&... args)
      : default_value_(std::move(default_value)), varray_(std::forward<Args>(args)...)
  {
  }

  FieldOutput<T> evaluate(IndexMask UNUSED(mask), const FieldInputs &UNUSED(inputs)) const override
  {
    return {varray_};
  }
};

class MultiFunctionField : public GField {
 private:
  Vector<std::shared_ptr<GField>> input_fields_;
  const MultiFunction *fn_;
  const int output_param_index_;

 public:
  MultiFunctionField(Vector<std::shared_ptr<GField>> input_fields,
                     const MultiFunction &fn,
                     const int output_param_index)
      : input_fields_(std::move(input_fields)), fn_(&fn), output_param_index_(output_param_index)
  {
  }

  const CPPType &output_type() const override
  {
    return fn_->param_type(output_param_index_).data_type().single_type();
  }

  GFieldOutput evaluate_generic(IndexMask mask, const FieldInputs &inputs) const
  {
    fn::MFParamsBuilder params{*fn_, mask.min_array_size()};
    fn::MFContextBuilder context;

    ResourceScope &scope = params.resource_scope();

    Vector<GMutableSpan> outputs;
    int output_span_index = -1;

    int input_index = 0;
    for (const int param_index : fn_->param_indices()) {
      fn::MFParamType param_type = fn_->param_type(param_index);
      switch (param_type.category()) {
        case fn::MFParamType::SingleInput: {
          const GField &field = *input_fields_[input_index];
          GFieldOutput &output = scope.add_value(field.evaluate_generic(mask, inputs), __func__);
          params.add_readonly_single_input(output.varray_ref());
          input_index++;
        }
        case fn::MFParamType::SingleOutput: {
          const CPPType &type = param_type.data_type().single_type();
          void *buffer = MEM_mallocN_aligned(
              mask.min_array_size() * type.size(), type.alignment(), __func__);
          GMutableSpan span{type, buffer, mask.min_array_size()};
          outputs.append(span);
          params.add_uninitialized_single_output(span);
          if (param_index == output_param_index_) {
            output_span_index = outputs.size() - 1;
          }
        }
        case fn::MFParamType::SingleMutable:
        case fn::MFParamType::VectorInput:
        case fn::MFParamType::VectorMutable:
        case fn::MFParamType::VectorOutput:
          BLI_assert_unreachable();
          break;
      }
    }

    fn_->call(mask, params, context);

    GMutableSpan output_span = outputs[output_span_index];
    outputs.remove(output_span_index);

    for (GMutableSpan span : outputs) {
      span.type().destruct_indices(span.data(), mask);
      MEM_freeN(span.data());
    }

    std::unique_ptr<GVArray> out_array = std::make_unique<fn::GVArray_For_OwnedGSpan>(output_span,
                                                                                      mask);
    return GFieldOutput{optional_ptr<const GVArray>{std::move(out_array)}};
  }
};

template<typename T> using FieldPtr = UserCounter<Field<T>>;

}  // namespace blender::bke