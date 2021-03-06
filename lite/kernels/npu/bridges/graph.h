// Copyright (c) 2019 PaddlePaddle Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "ai_ddk_lib/include/graph/op/all_ops.h"
#include "lite/core/op_lite.h"
#include "lite/core/tensor.h"

namespace paddle {
namespace lite {
namespace subgraph {
namespace npu {

// Type of graph nodes
class Type {
 public:
  Type(PrecisionType precision = PRECISION(kFloat),
       DataLayoutType layout = DATALAYOUT(kNCHW),
       bool persistable = false)
      : precision_(precision), layout_(layout), persistable_(persistable) {}

  void set_precision(PrecisionType precision) { precision_ = precision; }
  void set_layout(DataLayoutType layout) { layout_ = layout; }
  bool set_persistable(bool persistable) { persistable_ = persistable; }

  PrecisionType precision() const { return precision_; }
  DataLayoutType layout() const { return layout_; }
  bool persistable() const { return persistable_; }

 private:
  PrecisionType precision_{PRECISION(kFloat)};
  DataLayoutType layout_{DATALAYOUT(kNCHW)};
  bool persistable_{false};
};

// Graph to collect all of converted HiAI IR nodes
class Graph {
 public:
  template <typename T>
  std::shared_ptr<T> AddNode(const std::string& name,
                             PrecisionType precision = PRECISION(kFloat),
                             DataLayoutType layout = DATALAYOUT(kNCHW)) {
    auto unique_name = [&](const std::string& key) {
      int idx = 1;
      auto it = counts_.find(key);
      if (it == counts_.end()) {
        counts_.insert(std::make_pair(key, idx));
      } else {
        idx = ++(it->second);
      }
      return key + "_" + std::to_string(idx);
    };
    bool persistable = typeid(T) == typeid(ge::op::Const);
    auto it = nodes_.find(name);
    if (it != nodes_.end()) {
      // Only variable can rebind the name
      CHECK(!it->second.second.persistable() && !persistable)
          << "[NPU] Node " << name << " redefined.";
      // Generate a new unique name as the key to bind the origin node:
      // new_name->node
      nodes_.insert(std::make_pair(unique_name(name + "_var"), it->second));
      nodes_.erase(it);
    }
    // Create a new node and bind with the name: name->new_node
    auto node = std::make_shared<T>(unique_name(name + "_op"));
    nodes_.insert(std::make_pair(
        name, std::make_pair(node, Type(precision, layout, persistable))));
    return node;
  }

  // Const node
  std::shared_ptr<ge::op::Const> AddNode(
      const std::string& name,
      const Tensor& tensor,
      PrecisionType precision = PRECISION(kFloat),
      DataLayoutType layout = DATALAYOUT(kNCHW)) {
    return AddNode(name, tensor, tensor.dims().Vectorize(), precision, layout);
  }

  std::shared_ptr<ge::op::Const> AddNode(
      const std::string& name,
      const Tensor& tensor,
      std::vector<int64_t> shape,
      PrecisionType precision = PRECISION(kFloat),
      DataLayoutType layout = DATALAYOUT(kNCHW));

  std::shared_ptr<ge::op::Const> AddNode(
      const std::string& name,
      const Tensor& tensor,
      DDim dims,
      PrecisionType precision = PRECISION(kFloat),
      DataLayoutType layout = DATALAYOUT(kNCHW)) {
    return AddNode(name, tensor, dims.Vectorize(), precision, layout);
  }

  template <typename T>
  std::shared_ptr<ge::op::Const> AddNode(
      const std::string& name,
      const std::vector<T>& data,
      std::vector<int64_t> shape = {},
      DataLayoutType layout = DATALAYOUT(kNCHW)) {
    const std::type_info& info = typeid(T);
    PrecisionType precision = PRECISION(kFloat);
    if (info == typeid(float)) {
      precision = PRECISION(kFloat);
    } else if (info == typeid(int8_t)) {
      precision = PRECISION(kFloat);
    } else if (info == typeid(int32_t)) {
      precision = PRECISION(kInt32);
    } else {
      LOG(FATAL) << "[NPU] Unknow data type " << info.name();
    }
    if (shape.empty()) {
      shape = {static_cast<int64_t>(data.size())};
    } else {
      int size = 1;
      for (auto i : shape) {
        size *= i;
      }
      CHECK_EQ(data.size(), size);
    }
    Tensor tensor;
    tensor.Resize(shape);
    std::memcpy(reinterpret_cast<uint8_t*>(tensor.mutable_data<T>()),
                reinterpret_cast<const uint8_t*>(data.data()),
                data.size() * sizeof(T));
    return AddNode(name, tensor, precision, layout);
  }

  template <typename T>
  std::shared_ptr<ge::op::Const> AddNode(
      const std::string& name,
      const std::vector<T>& data,
      DDim dims,
      DataLayoutType layout = DATALAYOUT(kNCHW)) {
    return AddNode(name, data, dims.Vectorize(), layout);
  }

  template <typename T>
  std::shared_ptr<ge::op::Const> AddNode(
      const std::string& name,
      T value,
      std::vector<int64_t> shape = {1},
      DataLayoutType layout = DATALAYOUT(kNCHW)) {
    int64_t size = 1;
    for (auto i : shape) {
      size *= i;
    }
    std::vector<T> data(size, value);
    return AddNode(name, data, shape, layout);
  }

  template <typename T>
  std::shared_ptr<ge::op::Const> AddNode(
      const std::string& name,
      T value,
      DDim dims,
      DataLayoutType layout = DATALAYOUT(kNCHW)) {
    return AddNode(name, value, dims.Vectorize(), layout);
  }

  // Data node
  std::shared_ptr<ge::op::Data> AddNode(
      const std::string& name,
      std::vector<int64_t> shape,
      PrecisionType precision = PRECISION(kFloat),
      DataLayoutType layout = DATALAYOUT(kNCHW));

  std::shared_ptr<ge::op::Data> AddNode(
      const std::string& name,
      DDim dims,
      PrecisionType precision = PRECISION(kFloat),
      DataLayoutType layout = DATALAYOUT(kNCHW)) {
    return AddNode(name, dims.Vectorize(), precision, layout);
  }

  std::shared_ptr<ge::Operator> GetNode(std::string name) {
    CHECK(HasNode(name)) << "[NPU] Node " << name << " not found.";
    return nodes_.at(name).first;
  }

  const Type& GetType(const std::string& name) {
    CHECK(HasNode(name)) << "[NPU] Node " << name << " not found.";
    return nodes_.at(name).second;
  }

  bool HasNode(const std::string& name) {
    return nodes_.find(name) != nodes_.end();
  }

 private:
  std::unordered_map<std::string,
                     std::pair<std::shared_ptr<ge::Operator>, Type>>
      nodes_;
  std::unordered_map<std::string, int> counts_;
};

}  // namespace npu
}  // namespace subgraph
}  // namespace lite
}  // namespace paddle
