// Copyright (C) 2018-2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "pyopenvino/core/offline_transformations.hpp"

#include <pybind11/stl.h>

#include <compress_quantize_weights.hpp>
#include <generate_mapping_file.hpp>
#include <openvino/pass/make_stateful.hpp>
#include <openvino/pass/serialize.hpp>
#include <pot_transformations.hpp>
#include <pruning.hpp>
#include <transformations/common_optimizations/compress_float_constants.hpp>
#include <transformations/common_optimizations/mark_precision_sensitive_subgraphs.hpp>
#include <transformations/common_optimizations/moc_legacy_transformations.hpp>
#include <transformations/common_optimizations/moc_transformations.hpp>
#include <transformations/serialize.hpp>

#include "openvino/pass/low_latency.hpp"
#include "openvino/pass/manager.hpp"

namespace py = pybind11;

void regmodule_offline_transformations(py::module m) {
    py::module m_offline_transformations = m.def_submodule("offline_transformations", "Offline transformations module");
    m_offline_transformations.doc() = "openvino.offline_transformations module contains different offline passes.";

    m_offline_transformations.def(
        "apply_moc_transformations",
        [](std::shared_ptr<ov::Model> model, bool cf) {
            ov::pass::Manager manager;
            manager.register_pass<ngraph::pass::MOCTransformations>(cf);
            manager.run_passes(model);
        },
        py::arg("model"),
        py::arg("cf"));

    m_offline_transformations.def(
        "apply_moc_legacy_transformations",
        [](std::shared_ptr<ov::Model> model, const std::vector<std::string>& params_with_custom_types) {
            ov::pass::Manager manager;
            manager.register_pass<ov::pass::MOCLegacyTransformations>(params_with_custom_types);
            manager.run_passes(model);
        },
        py::arg("model"),
        py::arg("params_with_custom_types"));

    m_offline_transformations.def(
        "apply_pot_transformations",
        [](std::shared_ptr<ov::Model> model, std::string device) {
            ov::pass::Manager manager;
            manager.register_pass<ngraph::pass::POTTransformations>(std::move(device));
            manager.run_passes(model);
        },
        py::arg("model"),
        py::arg("device"));

    m_offline_transformations.def(
        "apply_low_latency_transformation",
        [](std::shared_ptr<ov::Model> model, bool use_const_initializer = true) {
            ov::pass::Manager manager;
            manager.register_pass<ov::pass::LowLatency2>(use_const_initializer);
            manager.run_passes(model);
        },
        py::arg("model"),
        py::arg("use_const_initializer") = true);

    m_offline_transformations.def(
        "apply_pruning_transformation",
        [](std::shared_ptr<ov::Model> model) {
            ov::pass::Manager manager;
            manager.register_pass<ngraph::pass::Pruning>();
            manager.run_passes(model);
        },
        py::arg("model"));

    m_offline_transformations.def(
        "generate_mapping_file",
        [](std::shared_ptr<ov::Model> model, std::string path, bool extract_names) {
            ov::pass::Manager manager;
            manager.register_pass<ngraph::pass::GenerateMappingFile>(path, extract_names);
            manager.run_passes(model);
        },
        py::arg("model"),
        py::arg("path"),
        py::arg("extract_names"));

    m_offline_transformations.def(
        "apply_make_stateful_transformation",
        [](std::shared_ptr<ov::Model> model, const std::map<std::string, std::string>& param_res_names) {
            ngraph::pass::Manager manager;
            manager.register_pass<ov::pass::MakeStateful>(param_res_names);
            manager.run_passes(model);
        },
        py::arg("model"),
        py::arg("param_res_names"));

    m_offline_transformations.def(
        "compress_model_transformation",
        [](std::shared_ptr<ov::Model> model) {
            ov::pass::Manager manager;
            manager.register_pass<ov::pass::MarkPrecisionSensitiveSubgraphs>();
            manager.register_pass<ov::pass::CompressFloatConstants>();
            manager.run_passes(model);
        },
        py::arg("model"));

    m_offline_transformations.def(
        "compress_quantize_weights_transformation",
        [](std::shared_ptr<ov::Model> model) {
            ov::pass::Manager manager;
            manager.register_pass<ngraph::pass::CompressQuantizeWeights>();
            manager.register_pass<ngraph::pass::ZeroPointOptimizer>();
            manager.run_passes(model);
        },
        py::arg("model"));
}
