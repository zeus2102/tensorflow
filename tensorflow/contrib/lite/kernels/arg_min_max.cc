/* Copyright 2018 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#include "tensorflow/contrib/lite/builtin_op_data.h"
#include "tensorflow/contrib/lite/context.h"
#include "tensorflow/contrib/lite/kernels/internal/optimized/optimized_ops.h"
#include "tensorflow/contrib/lite/kernels/internal/quantization_util.h"
#include "tensorflow/contrib/lite/kernels/internal/tensor.h"
#include "tensorflow/contrib/lite/kernels/kernel_util.h"
#include "tensorflow/contrib/lite/kernels/op_macros.h"

namespace tflite {
namespace ops {
namespace builtin {
namespace arg_min_max {

constexpr int kInputTensor = 0;
constexpr int kAxis = 1;
constexpr int kOutputTensor = 0;

TfLiteStatus Prepare(TfLiteContext* context, TfLiteNode* node) {
  TF_LITE_ENSURE_EQ(context, NumInputs(node), 2);
  TF_LITE_ENSURE_EQ(context, NumOutputs(node), 1);

  const TfLiteTensor* input = GetInput(context, node, kInputTensor);
  const TfLiteTensor* axis = GetInput(context, node, kAxis);
  // Make sure the axis is only 1 dimension.
  TF_LITE_ENSURE_EQ(context, NumElements(axis), 1);

  // Make sure the axis is only either int32 or int64.
  TF_LITE_ENSURE(context,
                 axis->type == kTfLiteInt32 || axis->type == kTfLiteInt64);
  TfLiteTensor* output = GetOutput(context, node, kOutputTensor);

  auto* params = reinterpret_cast<TfLiteArgMaxParams*>(node->builtin_data);
  switch (params->output_type) {
    case kTfLiteInt32:
      output->type = kTfLiteInt32;
      break;
    case kTfLiteInt64:
      output->type = kTfLiteInt64;
      break;
    default:
      context->ReportError(context, "Unknown index output data type: %d",
                           params->output_type);
      return kTfLiteError;
  }

  // Check conditions for different types.
  switch (input->type) {
    case kTfLiteFloat32:
    case kTfLiteUInt8:
    case kTfLiteInt32:
      break;

    default:
      context->ReportError(
          context,
          "Unkonwn input type: %d, only float32 and int types are supported",
          input->type);
      return kTfLiteError;
  }

  // Copy the input dimensions to output except make the last dimension 1.
  TF_LITE_ENSURE(context, NumDimensions(input) >= 1);
  TfLiteIntArray* output_size = TfLiteIntArrayCopy(input->dims);
  output_size->data[NumDimensions(input) - 1] = 1;

  return context->ResizeTensor(context, output, output_size);
}

template <typename T>
std::function<bool(T, T)> GetComparefunction(bool is_arg_max) {
  if (is_arg_max) {
    return std::greater<T>();
  } else {
    return std::less<T>();
  }
}

// The current impl actually ignores the axis argument.
// Only determine the index of the maximum value in the last dimension.
TfLiteStatus Eval(TfLiteContext* context, TfLiteNode* node, bool is_arg_max) {
  const TfLiteTensor* input = GetInput(context, node, kInputTensor);
  const TfLiteTensor* axis = GetInput(context, node, kAxis);
  TfLiteTensor* output = GetOutput(context, node, kOutputTensor);

#define TF_LITE_ARG_MIN_MAX(data_type, axis_type, output_type)         \
  optimized_ops::ArgMinMax(                                            \
      GetTensorData<axis_type>(axis), GetTensorData<data_type>(input), \
      GetTensorDims(input), GetTensorData<output_type>(output),        \
      GetTensorDims(output), GetComparefunction<data_type>(is_arg_max))
  if (axis->type == kTfLiteInt32) {
    switch (output->type) {
      case kTfLiteInt32: {
        switch (input->type) {
          case kTfLiteFloat32:
            TF_LITE_ARG_MIN_MAX(float, int32_t, int32_t);
            break;
          case kTfLiteUInt8:
            TF_LITE_ARG_MIN_MAX(uint8_t, int32_t, int32_t);
            break;
          case kTfLiteInt32:
            TF_LITE_ARG_MIN_MAX(int32_t, int32_t, int32_t);
            break;
          default:
            return kTfLiteError;
        }
      } break;
      case kTfLiteInt64: {
        switch (input->type) {
          case kTfLiteFloat32:
            TF_LITE_ARG_MIN_MAX(float, int32_t, int64_t);
            break;
          case kTfLiteUInt8:
            TF_LITE_ARG_MIN_MAX(uint8_t, int32_t, int64_t);
            break;
          case kTfLiteInt32:
            TF_LITE_ARG_MIN_MAX(int32_t, int32_t, int64_t);
            break;
          default:
            return kTfLiteError;
        }
      } break;
      default:
        return kTfLiteError;
    }
  } else {
    switch (output->type) {
      case kTfLiteInt32: {
        switch (input->type) {
          case kTfLiteFloat32:
            TF_LITE_ARG_MIN_MAX(float, int64_t, int32_t);
            break;
          case kTfLiteUInt8:
            TF_LITE_ARG_MIN_MAX(uint8_t, int64_t, int32_t);
            break;
          case kTfLiteInt32:
            TF_LITE_ARG_MIN_MAX(int32_t, int64_t, int32_t);
            break;
          default:
            return kTfLiteError;
        }
      } break;
      case kTfLiteInt64: {
        switch (input->type) {
          case kTfLiteFloat32:
            TF_LITE_ARG_MIN_MAX(float, int64_t, int64_t);
            break;
          case kTfLiteUInt8:
            TF_LITE_ARG_MIN_MAX(uint8_t, int64_t, int64_t);
            break;
          case kTfLiteInt32:
            TF_LITE_ARG_MIN_MAX(int32_t, int64_t, int64_t);
            break;
          default:
            return kTfLiteError;
        }
      } break;
      default:
        return kTfLiteError;
    }
  }
#undef TF_LITE_ARG_MIN_MAX

  return kTfLiteOk;
}

TfLiteStatus ArgMinEval(TfLiteContext* context, TfLiteNode* node) {
  return Eval(context, node, false);
}

TfLiteStatus ArgMaxEval(TfLiteContext* context, TfLiteNode* node) {
  return Eval(context, node, true);
}

}  // namespace arg_min_max

TfLiteRegistration* Register_ARG_MAX() {
  static TfLiteRegistration r = {nullptr, nullptr, arg_min_max::Prepare,
                                 arg_min_max::ArgMaxEval};
  return &r;
}

TfLiteRegistration* Register_ARG_MIN() {
  static TfLiteRegistration r = {nullptr, nullptr, arg_min_max::Prepare,
                                 arg_min_max::ArgMinEval};
  return &r;
}

}  // namespace builtin
}  // namespace ops
}  // namespace tflite
