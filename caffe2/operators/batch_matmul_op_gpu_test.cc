/**
 * Copyright (c) 2016-present, Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <memory>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "caffe2/core/context_gpu.h"
#include "caffe2/operators/batch_matmul_op.h"

namespace caffe2 {
namespace {

using testing::ElementsAreArray;
using testing::Test;

class BatchMatMulOpGPUTest : public Test {
 protected:
  void SetUp() override {
    if (!HasCudaGPU()) {
      return;
    }
    option_.set_device_type(CUDA);
    cuda_context_ = make_unique<CUDAContext>(option_);
    def_.set_name("test");
    def_.set_type("BatchMatMul");
    def_.add_input("A");
    def_.add_input("B");
    def_.add_output("Y");
    def_.mutable_device_option()->set_device_type(CUDA);
  }

  void AddConstInput(
      const std::vector<TIndex>& dims,
      const float value,
      const string& name) {
    Blob* blob = ws_.CreateBlob(name);
    auto* tensor = blob->GetMutable<Tensor<CUDAContext>>();
    tensor->Resize(dims);
    math::Set<float, CUDAContext>(
        tensor->size(),
        value,
        tensor->mutable_data<float>(),
        cuda_context_.get());
  }

  void VerifyOutput(const std::vector<TIndex>& dims, const float value) const {
    const Blob* Y_blob = ws_.GetBlob("Y");
    ASSERT_NE(nullptr, Y_blob);
    const auto& Y = Y_blob->Get<Tensor<CUDAContext>>();
    TensorCPU Y_cpu(Y);
    ASSERT_THAT(Y_cpu.dims(), ElementsAreArray(dims));
    for (int i = 0; i < Y_cpu.size(); ++i) {
      EXPECT_FLOAT_EQ(value, Y_cpu.data<float>()[i]);
    }
  }

  DeviceOption option_;
  std::unique_ptr<CUDAContext> cuda_context_;
  Workspace ws_;
  OperatorDef def_;
};

TEST_F(BatchMatMulOpGPUTest, BatchMatMulOpGPUNormalTest) {
  if (!HasCudaGPU()) {
    return;
  }
  AddConstInput(std::vector<TIndex>{3, 5, 10}, 1.0f, "A");
  AddConstInput(std::vector<TIndex>{3, 10, 6}, 1.0f, "B");
  std::unique_ptr<OperatorBase> op(CreateOperator(def_, &ws_));
  ASSERT_NE(nullptr, op);
  ASSERT_TRUE(op->Run());
  VerifyOutput(std::vector<TIndex>{3, 5, 6}, 10.0f);
}

TEST_F(BatchMatMulOpGPUTest, BatchMatMulOpGPUBroadcastTest) {
  if (!HasCudaGPU()) {
    return;
  }
  auto* arg = def_.add_arg();
  arg->set_name("broadcast");
  arg->set_i(1);
  AddConstInput(std::vector<TIndex>{3, 5, 10}, 1.0f, "A");
  AddConstInput(std::vector<TIndex>{2, 3, 10, 6}, 1.0f, "B");
  std::unique_ptr<OperatorBase> op(CreateOperator(def_, &ws_));
  ASSERT_NE(nullptr, op);
  ASSERT_TRUE(op->Run());
  VerifyOutput(std::vector<TIndex>{2, 3, 5, 6}, 10.0f);
}

} // namespace
} // namespace caffe2
