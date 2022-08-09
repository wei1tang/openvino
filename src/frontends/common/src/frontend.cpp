// Copyright (C) 2018-2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include <openvino/util/env_util.hpp>
#include <openvino/util/file_util.hpp>

#include "openvino/frontend/exception.hpp"
#include "openvino/frontend/extension/op.hpp"
#include "openvino/frontend/manager.hpp"
#include "openvino/frontend/place.hpp"
#include "plugin_loader.hpp"
#include "so_extension.hpp"
#include "utils.hpp"

using namespace ov;
using namespace ov::frontend;

std::shared_ptr<ov::Model> FrontEnd::create_copy(const std::shared_ptr<ov::Model>& ov_model,
                                                 const std::shared_ptr<void>& shared_object) {
    // Recreate ov::Model using main runtime, not FrontEnd's one
    auto copy = std::make_shared<Model>(ov_model->get_results(),
                                        ov_model->get_sinks(),
                                        ov_model->get_parameters(),
                                        ov_model->get_variables(),
                                        ov_model->get_friendly_name());
    copy->m_shared_object = shared_object;
    copy->get_rt_info() = ov_model->get_rt_info();
    return copy;
}

FrontEnd::FrontEnd() = default;

FrontEnd::~FrontEnd() = default;

bool FrontEnd::supported_impl(const std::vector<ov::Any>& variants) const {
    if (m_actual) {
        return m_actual->supported_impl(variants);
    }
    return false;
}

InputModel::Ptr FrontEnd::load_impl(const std::vector<ov::Any>& variants) const {
    FRONT_END_CHECK_IMPLEMENTED(m_actual, load_impl);
    auto model = std::make_shared<InputModel>();
    model->m_shared_object = m_shared_object;
    model->m_actual = m_actual->load_impl(variants);
    return model;
}

std::shared_ptr<ov::Model> FrontEnd::convert(const InputModel::Ptr& model) const {
    FRONT_END_CHECK_IMPLEMENTED(m_actual, convert);
    return FrontEnd::create_copy(m_actual->convert(model->m_actual), m_shared_object);
}

void FrontEnd::convert(const std::shared_ptr<Model>& model) const {
    FRONT_END_CHECK_IMPLEMENTED(m_actual, convert);
    m_actual->convert(model);
}

std::shared_ptr<Model> FrontEnd::convert_partially(const InputModel::Ptr& model) const {
    FRONT_END_CHECK_IMPLEMENTED(m_actual, convert_partially);
    return FrontEnd::create_copy(m_actual->convert_partially(model->m_actual), m_shared_object);
}

std::shared_ptr<Model> FrontEnd::decode(const InputModel::Ptr& model) const {
    FRONT_END_CHECK_IMPLEMENTED(m_actual, decode);
    return FrontEnd::create_copy(m_actual->decode(model->m_actual), m_shared_object);
}

void FrontEnd::normalize(const std::shared_ptr<Model>& model) const {
    FRONT_END_CHECK_IMPLEMENTED(m_actual, normalize);
    m_actual->normalize(model);
}

void FrontEnd::add_extension(const std::shared_ptr<ov::Extension>& extension) {
    if (m_actual) {
        add_extension_to_shared_data(m_shared_object, extension);
        m_actual->add_extension(extension);
        return;
    }
    // Left unimplemented intentionally.
    // Each frontend can support own set of extensions, so this method should be implemented on the frontend side
}

void FrontEnd::add_extension(const std::vector<std::shared_ptr<ov::Extension>>& extensions) {
    for (const auto& ext : extensions) {
        add_extension(ext);
    }
}

void FrontEnd::add_extension(const std::string& library_path) {
    add_extension(ov::detail::load_extensions(library_path));
}

#ifdef OPENVINO_ENABLE_UNICODE_PATH_SUPPORT
void FrontEnd::add_extension(const std::wstring& library_path) {
    add_extension(ov::detail::load_extensions(library_path));
}
#endif

std::string FrontEnd::get_name() const {
    if (!m_actual) {
        return {};
    }
    return m_actual->get_name();
}
