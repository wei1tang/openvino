// Copyright (C) 2018-2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include <pybind11/pybind11.h>

#include <openvino/core/graph_util.hpp>
#include <openvino/core/model.hpp>
#include <openvino/core/node.hpp>
#include <openvino/core/version.hpp>
#include <string>

#include "openvino/runtime/core.hpp"
#include "pyopenvino/graph/axis_set.hpp"
#include "pyopenvino/graph/axis_vector.hpp"
#include "pyopenvino/graph/coordinate.hpp"
#include "pyopenvino/graph/coordinate_diff.hpp"
#include "pyopenvino/graph/model.hpp"
#include "pyopenvino/graph/node.hpp"
#include "pyopenvino/graph/node_factory.hpp"
#include "pyopenvino/graph/node_input.hpp"
#include "pyopenvino/graph/node_output.hpp"
#if defined(ENABLE_OV_ONNX_FRONTEND)
#    include "pyopenvino/graph/onnx_import/onnx_import.hpp"
#endif
#include "pyopenvino/core/async_infer_queue.hpp"
#include "pyopenvino/core/compiled_model.hpp"
#include "pyopenvino/core/containers.hpp"
#include "pyopenvino/core/core.hpp"
#include "pyopenvino/core/extension.hpp"
#include "pyopenvino/core/ie_parameter.hpp"
#include "pyopenvino/core/infer_request.hpp"
#include "pyopenvino/core/offline_transformations.hpp"
#include "pyopenvino/core/profiling_info.hpp"
#include "pyopenvino/core/tensor.hpp"
#include "pyopenvino/core/variable_state.hpp"
#include "pyopenvino/core/version.hpp"
#include "pyopenvino/frontend/extension.hpp"
#include "pyopenvino/frontend/frontend.hpp"
#include "pyopenvino/frontend/input_model.hpp"
#include "pyopenvino/frontend/manager.hpp"
#include "pyopenvino/frontend/node_context.hpp"
#include "pyopenvino/frontend/place.hpp"
#include "pyopenvino/graph/any.hpp"
#include "pyopenvino/graph/descriptors/tensor.hpp"
#include "pyopenvino/graph/dimension.hpp"
#include "pyopenvino/graph/discrete_type_info.hpp"
#include "pyopenvino/graph/layout.hpp"
#include "pyopenvino/graph/layout_helpers.hpp"
#include "pyopenvino/graph/ops/constant.hpp"
#include "pyopenvino/graph/ops/if.hpp"
#include "pyopenvino/graph/ops/loop.hpp"
#include "pyopenvino/graph/ops/parameter.hpp"
#include "pyopenvino/graph/ops/result.hpp"
#include "pyopenvino/graph/ops/tensor_iterator.hpp"
#include "pyopenvino/graph/ops/util/regmodule_graph_op_util.hpp"
#include "pyopenvino/graph/partial_shape.hpp"
#include "pyopenvino/graph/passes/regmodule_graph_passes.hpp"
#include "pyopenvino/graph/preprocess/pre_post_process.hpp"
#include "pyopenvino/graph/rt_map.hpp"
#include "pyopenvino/graph/shape.hpp"
#include "pyopenvino/graph/strides.hpp"
#include "pyopenvino/graph/types/regmodule_graph_types.hpp"
#include "pyopenvino/graph/util.hpp"

namespace py = pybind11;

std::string get_version() {
    auto version = ov::get_openvino_version();
    return version.buildNumber;
}

PYBIND11_MODULE(pyopenvino, m) {
    m.doc() = "Package openvino.pyopenvino which wraps openvino C++ APIs";
    std::string pyopenvino_version = CI_BUILD_NUMBER;
    std::string runtime_version = get_version();
    bool is_custom_pyopenvino_version = pyopenvino_version.empty() || pyopenvino_version.find("custom_") == 0;
    bool is_custom_runtime_version = runtime_version.empty() || runtime_version.find("custom_") == 0;
    auto versions_compatible =
        is_custom_pyopenvino_version || is_custom_runtime_version || pyopenvino_version == runtime_version;
    OPENVINO_ASSERT(versions_compatible,
                    "OpenVINO Python version (",
                    pyopenvino_version,
                    ") mismatches with OpenVINO Runtime library version (",
                    runtime_version,
                    "). It can happen if you have 2 or more different versions of OpenVINO installed in system. "
                    "Please ensure that environment variables (e.g. PATH, PYTHONPATH) are set correctly so that "
                    "OpenVINO Runtime and Python libraries point to same release.");

    m.def("get_version", &get_version);
    m.def("get_batch", &ov::get_batch);
    m.def("set_batch", &ov::set_batch);
    m.def(
        "set_batch",
        [](const std::shared_ptr<ov::Model>& model, int64_t value) {
            return ov::set_batch(model, ov::Dimension(value));
        },
        py::arg("model"),
        py::arg("batch_size") = -1);

    m.def(
        "serialize",
        [](std::shared_ptr<ov::Model>& model,
           const std::string& xml_path,
           const std::string& bin_path,
           const std::string& version) {
            ov::serialize(model, xml_path, bin_path, Common::convert_to_version(version));
        },
        py::arg("model"),
        py::arg("xml_path"),
        py::arg("bin_path") = "",
        py::arg("version") = "UNSPECIFIED",
        R"(
            Serialize given model into IR. The generated .xml and .bin files will be saved
            into provided paths.
            :param model: model which will be converted to IR representation
            :type model: openvino.runtime.Model
            :param xml_path: path where .xml file will be saved
            :type xml_path: str
            :param bin_path: path where .bin file will be saved (optional),
                             the same name as for xml_path will be used by default.
            :type bin_path: str
            :param version: version of the generated IR (optional).
            Supported versions are:
            - "UNSPECIFIED" (default) : Use the latest or model version
            - "IR_V10" : v10 IR
            - "IR_V11" : v11 IR

            :Examples:

            1. Default IR version:

            .. code-block:: python

                shape = [2, 2]
                parameter_a = ov.parameter(shape, dtype=np.float32, name="A")
                parameter_b = ov.parameter(shape, dtype=np.float32, name="B")
                parameter_c = ov.parameter(shape, dtype=np.float32, name="C")
                op = (parameter_a + parameter_b) * parameter_c
                model = Model(op, [parameter_a, parameter_b, parameter_c], "Model")
                # IR generated with default version
                serialize(model, xml_path="./serialized.xml", bin_path="./serialized.bin")
            2. IR version 11:

            .. code-block:: python
                parameter_a = ov.parameter(shape, dtype=np.float32, name="A")
                parameter_b = ov.parameter(shape, dtype=np.float32, name="B")
                parameter_c = ov.parameter(shape, dtype=np.float32, name="C")
                op = (parameter_a + parameter_b) * parameter_c
                model = Model(ops, [parameter_a, parameter_b, parameter_c], "Model")
                # IR generated with default version
                serialize(model, xml_path="./serialized.xml", bin_path="./serialized.bin", version="IR_V11")
        )");

    m.def("shutdown",
          &ov::shutdown,
          R"(
                    Shut down the OpenVINO by deleting all static-duration objects allocated by the library and releasing
                    dependent resources

                    This function should be used by advanced user to control unload the resources.

                    You might want to use this function if you are developing a dynamically-loaded library which should clean up all
                    resources after itself when the library is unloaded.
                )");

    regclass_graph_PyRTMap(m);
    regmodule_graph_types(m);
    regclass_graph_Dimension(m);  // Dimension must be registered before PartialShape
    regclass_graph_Layout(m);
    regclass_graph_Shape(m);
    regclass_graph_PartialShape(m);
    regclass_graph_Node(m);
    regclass_graph_Input(m);
    regclass_graph_NodeFactory(m);
    regclass_graph_Strides(m);
    regclass_graph_CoordinateDiff(m);
    regclass_graph_AxisSet(m);
    regclass_graph_AxisVector(m);
    regclass_graph_Coordinate(m);
    regclass_graph_descriptor_Tensor(m);
    regclass_graph_DiscreteTypeInfo(m);
    py::module m_op = m.def_submodule("op", "Package ngraph.impl.op that wraps ov::op");  // TODO(!)
    regclass_graph_op_Constant(m_op);
    regclass_graph_op_Parameter(m_op);
    regclass_graph_op_Result(m_op);
    regclass_graph_op_If(m_op);
    regclass_graph_op_Loop(m_op);
    regclass_graph_op_TensorIterator(m_op);

#if defined(ENABLE_OV_ONNX_FRONTEND)
    regmodule_graph_onnx_import(m);
#endif
    regmodule_graph_op_util(m_op);
    py::module m_preprocess =
        m.def_submodule("preprocess", "Package openvino.runtime.preprocess that wraps ov::preprocess");
    regclass_graph_PrePostProcessor(m_preprocess);
    regclass_graph_Model(m);
    regmodule_graph_passes(m);
    regmodule_graph_util(m);
    regmodule_graph_layout_helpers(m);
    regclass_graph_Any(m);
    regclass_graph_Output<ov::Node>(m, std::string(""));
    regclass_graph_Output<const ov::Node>(m, std::string("Const"));

    regclass_Core(m);
    regclass_Tensor(m);
    // Registering specific types of containers
    Containers::regclass_TensorIndexMap(m);
    Containers::regclass_TensorNameMap(m);

    regclass_CompiledModel(m);
    regclass_InferRequest(m);
    regclass_VariableState(m);
    regclass_Version(m);
    regclass_Parameter(m);
    regclass_AsyncInferQueue(m);
    regclass_ProfilingInfo(m);
    regclass_Extension(m);

    // frontend
    regclass_frontend_Place(m);
    regclass_frontend_InitializationFailureFrontEnd(m);
    regclass_frontend_GeneralFailureFrontEnd(m);
    regclass_frontend_OpConversionFailureFrontEnd(m);
    regclass_frontend_OpValidationFailureFrontEnd(m);
    regclass_frontend_NotImplementedFailureFrontEnd(m);
    regclass_frontend_FrontEndManager(m);
    regclass_frontend_FrontEnd(m);
    regclass_frontend_InputModel(m);
    regclass_frontend_NodeContext(m);

    // frontend extensions
    regclass_frontend_TelemetryExtension(m);
    regclass_frontend_DecoderTransformationExtension(m);
    regclass_frontend_JsonConfigExtension(m);
    regclass_frontend_ConversionExtensionBase(m);
    regclass_frontend_ConversionExtension(m);
    regclass_frontend_ProgressReporterExtension(m);
    regclass_frontend_OpExtension(m);

    // transformations
    regmodule_offline_transformations(m);
}
