#include "mesh_io.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <map>
#include <iomanip>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <chrono>
#include <system_error>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <OleAuto.h>
#endif

namespace nbcam {

void TriangleMesh::clear() {
    vertices.clear();
    triangles.clear();
}

bool TriangleMesh::isValid() const {
    if (vertices.empty() || triangles.empty()) {
        return false;
    }
    
    // 检查索引有效性
    size_t max_index = vertices.size() - 1;
    for (const auto& tri : triangles) {
        if (tri.v0 > max_index || tri.v1 > max_index || tri.v2 > max_index) {
            return false;
        }
    }
    
    return true;
}

std::unique_ptr<TriangleMesh> MeshIO::loadFromFile(const std::string& filepath) {
    const auto dot_pos = filepath.find_last_of('.');
    if (dot_pos == std::string::npos || dot_pos + 1 >= filepath.size()) {
        spdlog::error("不支持的文件格式: {}", filepath);
        return nullptr;
    }

    std::string ext = filepath.substr(dot_pos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext == "obj") {
        return loadOBJ(filepath);
    } else if (ext == "stl") {
        return loadSTL(filepath);
    } else if (ext == "sldprt") {
        return loadSLDPRT(filepath);
    } else {
        spdlog::error("不支持的文件格式: {}", ext);
        return nullptr;
    }
}

bool MeshIO::saveToFile(const TriangleMesh& mesh, const std::string& filepath) {
    const auto dot_pos = filepath.find_last_of('.');
    if (dot_pos == std::string::npos || dot_pos + 1 >= filepath.size()) {
        spdlog::error("不支持的文件格式: {}", filepath);
        return false;
    }

    std::string ext = filepath.substr(dot_pos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext == "obj") {
        return saveOBJ(mesh, filepath);
    } else if (ext == "stl") {
        return saveSTL(mesh, filepath);
    } else {
        spdlog::error("不支持的文件格式: {}", ext);
        return false;
    }
}

#ifdef _WIN32
namespace {

class ComApartment {
public:
    ComApartment()
        : hr_(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)),
          needs_uninitialize_(SUCCEEDED(hr_))
    {
    }

    ~ComApartment()
    {
        if (needs_uninitialize_) {
            CoUninitialize();
        }
    }

    bool isUsable() const
    {
        return SUCCEEDED(hr_) || hr_ == RPC_E_CHANGED_MODE;
    }

    HRESULT result() const { return hr_; }

private:
    HRESULT hr_ = E_FAIL;
    bool needs_uninitialize_ = false;
};

class DispatchPtr {
public:
    DispatchPtr() = default;
    explicit DispatchPtr(IDispatch* dispatch) : dispatch_(dispatch) {}

    ~DispatchPtr()
    {
        reset();
    }

    DispatchPtr(const DispatchPtr&) = delete;
    DispatchPtr& operator=(const DispatchPtr&) = delete;

    DispatchPtr(DispatchPtr&& other) noexcept
        : dispatch_(other.dispatch_)
    {
        other.dispatch_ = nullptr;
    }

    DispatchPtr& operator=(DispatchPtr&& other) noexcept
    {
        if (this != &other) {
            reset(other.dispatch_);
            other.dispatch_ = nullptr;
        }
        return *this;
    }

    IDispatch* get() const { return dispatch_; }
    explicit operator bool() const { return dispatch_ != nullptr; }

    void reset(IDispatch* dispatch = nullptr)
    {
        if (dispatch_) {
            dispatch_->Release();
        }
        dispatch_ = dispatch;
    }

private:
    IDispatch* dispatch_ = nullptr;
};

std::wstring toWideString(const std::string& value)
{
    if (value.empty()) {
        return {};
    }

    const int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.c_str(), -1, nullptr, 0);
    if (required <= 0) {
        return {};
    }

    std::wstring wide(static_cast<size_t>(required - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.c_str(), -1, wide.data(), required);
    return wide;
}

std::string toUtf8String(const std::wstring& value)
{
    if (value.empty()) {
        return {};
    }

    const int required = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return {};
    }

    std::string utf8(static_cast<size_t>(required - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, utf8.data(), required, nullptr, nullptr);
    return utf8;
}

std::string bstrToUtf8(BSTR value)
{
    if (!value) {
        return {};
    }
    return toUtf8String(std::wstring(value, SysStringLen(value)));
}

void initBstrVariant(VARIANT& variant, const std::wstring& value)
{
    VariantInit(&variant);
    variant.vt = VT_BSTR;
    variant.bstrVal = SysAllocStringLen(value.data(), static_cast<UINT>(value.size()));
}

void initIntVariant(VARIANT& variant, long value)
{
    VariantInit(&variant);
    variant.vt = VT_I4;
    variant.lVal = value;
}

void initBoolVariant(VARIANT& variant, bool value)
{
    VariantInit(&variant);
    variant.vt = VT_BOOL;
    variant.boolVal = value ? VARIANT_TRUE : VARIANT_FALSE;
}

void initIntByRefVariant(VARIANT& variant, long& value)
{
    VariantInit(&variant);
    variant.vt = VT_BYREF | VT_I4;
    variant.plVal = &value;
}

void initNullVariant(VARIANT& variant)
{
    VariantInit(&variant);
    variant.vt = VT_NULL;
}

void clearVariants(VARIANT* variants, UINT count)
{
    if (!variants) {
        return;
    }
    for (UINT i = 0; i < count; ++i) {
        VariantClear(&variants[i]);
    }
}

HRESULT invokeDispatch(IDispatch* dispatch,
                       const wchar_t* name,
                       WORD flags,
                       VARIANT* reversed_args,
                       UINT arg_count,
                       VARIANT* result = nullptr,
                       bool log_failure = true)
{
    if (!dispatch || !name) {
        return E_POINTER;
    }

    DISPID dispid = 0;
    LPOLESTR ole_name = const_cast<LPOLESTR>(name);
    HRESULT hr = dispatch->GetIDsOfNames(IID_NULL, &ole_name, 1, LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) {
        if (log_failure) {
            spdlog::debug("SolidWorks COM: GetIDsOfNames failed for {} (hr=0x{:08X})",
                          toUtf8String(name),
                          static_cast<unsigned>(hr));
        }
        return hr;
    }

    DISPID property_put = DISPID_PROPERTYPUT;
    DISPPARAMS params{};
    params.rgvarg = reversed_args;
    params.cArgs = arg_count;
    if (flags & DISPATCH_PROPERTYPUT) {
        params.rgdispidNamedArgs = &property_put;
        params.cNamedArgs = 1;
    }

    EXCEPINFO excep{};
    UINT arg_error = 0;
    if (result) {
        VariantInit(result);
    }

    hr = dispatch->Invoke(dispid,
                          IID_NULL,
                          LOCALE_USER_DEFAULT,
                          flags,
                          &params,
                          result,
                          &excep,
                          &arg_error);

    if (FAILED(hr) && log_failure) {
        const std::string description = bstrToUtf8(excep.bstrDescription);
        if (!description.empty()) {
            spdlog::warn("SolidWorks COM: {} failed (hr=0x{:08X}, arg={}, {})",
                         toUtf8String(name),
                         static_cast<unsigned>(hr),
                         arg_error,
                         description);
        } else {
            spdlog::warn("SolidWorks COM: {} failed (hr=0x{:08X}, arg={})",
                         toUtf8String(name),
                         static_cast<unsigned>(hr),
                         arg_error);
        }
    }

    if (excep.bstrSource) {
        SysFreeString(excep.bstrSource);
    }
    if (excep.bstrDescription) {
        SysFreeString(excep.bstrDescription);
    }
    if (excep.bstrHelpFile) {
        SysFreeString(excep.bstrHelpFile);
    }
    return hr;
}

DispatchPtr dispatchFromVariant(VARIANT& variant)
{
    if (variant.vt == VT_DISPATCH && variant.pdispVal) {
        IDispatch* dispatch = variant.pdispVal;
        variant.vt = VT_EMPTY;
        variant.pdispVal = nullptr;
        return DispatchPtr(dispatch);
    }

    if (variant.vt == VT_UNKNOWN && variant.punkVal) {
        IDispatch* dispatch = nullptr;
        if (SUCCEEDED(variant.punkVal->QueryInterface(IID_IDispatch, reinterpret_cast<void**>(&dispatch)))) {
            return DispatchPtr(dispatch);
        }
    }

    return {};
}

DispatchPtr getPropertyDispatch(IDispatch* dispatch, const wchar_t* name)
{
    VARIANT result;
    const HRESULT hr = invokeDispatch(dispatch, name, DISPATCH_PROPERTYGET, nullptr, 0, &result);
    if (FAILED(hr)) {
        VariantClear(&result);
        return {};
    }

    DispatchPtr property = dispatchFromVariant(result);
    VariantClear(&result);
    return property;
}

std::wstring getMethodString(IDispatch* dispatch, const wchar_t* name)
{
    VARIANT result;
    const HRESULT hr = invokeDispatch(dispatch, name, DISPATCH_METHOD, nullptr, 0, &result, false);
    if (FAILED(hr) || result.vt != VT_BSTR) {
        VariantClear(&result);
        return {};
    }

    std::wstring value(result.bstrVal, SysStringLen(result.bstrVal));
    VariantClear(&result);
    return value;
}

bool setBoolProperty(IDispatch* dispatch, const wchar_t* name, bool value)
{
    VARIANT arg;
    initBoolVariant(arg, value);
    const HRESULT hr = invokeDispatch(dispatch, name, DISPATCH_PROPERTYPUT, &arg, 1, nullptr, false);
    VariantClear(&arg);
    return SUCCEEDED(hr);
}

bool variantIndicatesSuccess(const VARIANT& variant)
{
    if (variant.vt == VT_BOOL) {
        return variant.boolVal == VARIANT_TRUE;
    }
    if (variant.vt == VT_I4) {
        return variant.lVal != 0;
    }
    if (variant.vt == VT_EMPTY || variant.vt == VT_NULL) {
        return true;
    }
    return false;
}

long variantToLong(const VARIANT& variant, long fallback = 0)
{
    if (variant.vt == VT_I4) {
        return variant.lVal;
    }
    if (variant.vt == VT_I2) {
        return variant.iVal;
    }
    if (variant.vt == VT_INT) {
        return variant.intVal;
    }
    if (variant.vt == VT_UI4) {
        return static_cast<long>(variant.ulVal);
    }
    if (variant.vt == VT_R8) {
        return static_cast<long>(variant.dblVal);
    }
    return fallback;
}

long getSolidWorksDocUnitCode(IDispatch* model_doc)
{
    VARIANT result;
    const HRESULT hr = invokeDispatch(model_doc, L"GetUnits", DISPATCH_METHOD, nullptr, 0, &result, false);
    if (FAILED(hr) || !(result.vt & VT_ARRAY) || !result.parray) {
        VariantClear(&result);
        return -1;
    }

    long lower_bound = 0;
    long upper_bound = -1;
    SafeArrayGetLBound(result.parray, 1, &lower_bound);
    SafeArrayGetUBound(result.parray, 1, &upper_bound);
    if (upper_bound < lower_bound) {
        VariantClear(&result);
        return -1;
    }

    VARIANT item;
    VariantInit(&item);
    long index = lower_bound;
    const HRESULT item_hr = SafeArrayGetElement(result.parray, &index, &item);
    const long unit_code = SUCCEEDED(item_hr) ? variantToLong(item, -1) : -1;
    VariantClear(&item);
    VariantClear(&result);
    return unit_code;
}

double solidWorksLengthUnitToMillimeters(long unit_code)
{
    switch (unit_code) {
    case 0: return 25.4;    // inches
    case 1: return 304.8;   // feet
    case 2: return 1000.0;  // meters
    case 3: return 10.0;    // centimeters
    case 4: return 1.0;     // millimeters
    case 5: return 1e-3;    // microns
    case 6: return 1e-6;    // nanometers
    default:
        return 1.0;
    }
}

void scaleMesh(nbcam::TriangleMesh& mesh, double scale)
{
    if (!std::isfinite(scale) || std::abs(scale - 1.0) < 1e-12) {
        return;
    }

    for (auto& vertex : mesh.vertices) {
        vertex.x *= scale;
        vertex.y *= scale;
        vertex.z *= scale;
    }
}

bool activateSolidWorksDocument(IDispatch* solidworks_app, const std::wstring& title)
{
    if (!solidworks_app || title.empty()) {
        return false;
    }

    long errors = 0;
    VARIANT args[3];
    initIntByRefVariant(args[0], errors);
    initBoolVariant(args[1], true);
    initBstrVariant(args[2], title);

    VARIANT result;
    const HRESULT hr = invokeDispatch(solidworks_app, L"ActivateDoc3", DISPATCH_METHOD, args, 3, &result, false);
    const bool success = SUCCEEDED(hr) && errors == 0;
    VariantClear(&result);
    clearVariants(args, 3);
    return success;
}

bool saveModelDocAsStl(IDispatch* model_doc, const std::wstring& stl_path)
{
    VARIANT args[3];
    initIntVariant(args[0], 1);  // swSaveAsOptions_Silent
    initIntVariant(args[1], 0);  // swSaveAsCurrentVersion
    initBstrVariant(args[2], stl_path);

    VARIANT result;
    const HRESULT hr = invokeDispatch(model_doc, L"SaveAs3", DISPATCH_METHOD, args, 3, &result);
    const bool success = SUCCEEDED(hr) && variantIndicatesSuccess(result);
    VariantClear(&result);
    clearVariants(args, 3);

    return success;
}

DispatchPtr startSolidWorks()
{
    CLSID clsid{};
    HRESULT hr = CLSIDFromProgID(L"SldWorks.Application", &clsid);
    if (FAILED(hr)) {
        spdlog::error("未找到SolidWorks.Application COM组件，无法导入SLDPRT");
        return {};
    }

    IDispatch* dispatch = nullptr;
    hr = CoCreateInstance(clsid,
                          nullptr,
                          CLSCTX_LOCAL_SERVER,
                          IID_IDispatch,
                          reinterpret_cast<void**>(&dispatch));
    if (FAILED(hr) || !dispatch) {
        spdlog::error("启动SolidWorks失败，无法导入SLDPRT (hr=0x{:08X})", static_cast<unsigned>(hr));
        return {};
    }

    setBoolProperty(dispatch, L"Visible", false);
    return DispatchPtr(dispatch);
}

DispatchPtr openSolidWorksPart(IDispatch* solidworks_app,
                               const std::wstring& source_path,
                               long& errors,
                               long& warnings)
{
    errors = 0;
    warnings = 0;

    VARIANT args[6];
    initIntByRefVariant(args[0], warnings);
    initIntByRefVariant(args[1], errors);
    initBstrVariant(args[2], L"");
    initIntVariant(args[3], 1);  // swOpenDocOptions_Silent
    initIntVariant(args[4], 1);  // swDocPART
    initBstrVariant(args[5], source_path);

    VARIANT result;
    const HRESULT hr = invokeDispatch(solidworks_app, L"OpenDoc6", DISPATCH_METHOD, args, 6, &result);
    clearVariants(args, 6);
    if (FAILED(hr)) {
        VariantClear(&result);
        return {};
    }

    DispatchPtr document = dispatchFromVariant(result);
    VariantClear(&result);
    return document;
}

void closeSolidWorksDocument(IDispatch* solidworks_app, IDispatch* model_doc)
{
    std::wstring title = getMethodString(model_doc, L"GetTitle");
    if (title.empty()) {
        return;
    }

    VARIANT arg;
    initBstrVariant(arg, title);
    invokeDispatch(solidworks_app, L"CloseDoc", DISPATCH_METHOD, &arg, 1, nullptr, false);
    VariantClear(&arg);
}

void exitSolidWorks(IDispatch* solidworks_app)
{
    invokeDispatch(solidworks_app, L"ExitApp", DISPATCH_METHOD, nullptr, 0, nullptr, false);
}

std::filesystem::path makeTemporaryStlPath(const std::filesystem::path& source_path)
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    const DWORD pid = GetCurrentProcessId();
    const std::wstring suffix = L"_nbcam_" + std::to_wstring(pid) + L"_" + std::to_wstring(now) + L".stl";
    return std::filesystem::temp_directory_path() / (source_path.stem().wstring() + suffix);
}

std::string pathToUtf8String(const std::filesystem::path& path)
{
    return path.u8string();
}

}  // namespace

std::unique_ptr<TriangleMesh> MeshIO::loadSLDPRT(const std::string& filepath) {
    ComApartment com;
    if (!com.isUsable()) {
        spdlog::error("初始化COM失败，无法导入SLDPRT (hr=0x{:08X})", static_cast<unsigned>(com.result()));
        return nullptr;
    }

    const std::filesystem::path source_path = std::filesystem::absolute(std::filesystem::u8path(filepath));
    if (!std::filesystem::exists(source_path)) {
        spdlog::error("无法打开SLDPRT文件: {}", filepath);
        return nullptr;
    }

    const std::wstring source_wide = source_path.wstring();
    const std::filesystem::path temp_stl = makeTemporaryStlPath(source_path);
    const std::wstring temp_stl_wide = temp_stl.wstring();

    std::error_code ec;
    std::filesystem::remove(temp_stl, ec);

    DispatchPtr solidworks_app = startSolidWorks();
    if (!solidworks_app) {
        return nullptr;
    }

    long open_errors = 0;
    long open_warnings = 0;
    DispatchPtr model_doc = openSolidWorksPart(solidworks_app.get(),
                                               source_wide,
                                               open_errors,
                                               open_warnings);
    if (!model_doc) {
        spdlog::error("SolidWorks打开SLDPRT失败: {}, errors={}, warnings={}",
                      filepath,
                      open_errors,
                      open_warnings);
        exitSolidWorks(solidworks_app.get());
        return nullptr;
    }

    const long unit_code = getSolidWorksDocUnitCode(model_doc.get());
    const double unit_to_mm = solidWorksLengthUnitToMillimeters(unit_code);
    const std::wstring title = getMethodString(model_doc.get(), L"GetTitle");
    if (!activateSolidWorksDocument(solidworks_app.get(), title)) {
        spdlog::warn("SolidWorks激活文档失败，继续尝试导出STL: {}", filepath);
    }

    const bool saved = saveModelDocAsStl(model_doc.get(), temp_stl_wide);
    closeSolidWorksDocument(solidworks_app.get(), model_doc.get());
    exitSolidWorks(solidworks_app.get());

    if (!saved || !std::filesystem::exists(temp_stl) || std::filesystem::file_size(temp_stl, ec) == 0) {
        spdlog::error("SolidWorks导出临时STL失败: {}", pathToUtf8String(temp_stl));
        std::filesystem::remove(temp_stl, ec);
        return nullptr;
    }

    auto mesh = loadSTL(pathToUtf8String(temp_stl));
    std::filesystem::remove(temp_stl, ec);
    if (!mesh || !mesh->isValid()) {
        spdlog::error("SLDPRT转换后的STL读取失败: {}", filepath);
        return nullptr;
    }
    scaleMesh(*mesh, unit_to_mm);

    spdlog::info("加载SLDPRT文件成功: {} 顶点, {} 三角面, unit_code={}, scale_to_mm={}",
                 mesh->vertices.size(),
                 mesh->triangles.size(),
                 unit_code,
                 unit_to_mm);
    return mesh;
}
#else
std::unique_ptr<TriangleMesh> MeshIO::loadSLDPRT(const std::string& filepath) {
    spdlog::error("当前平台不支持SLDPRT导入: {}", filepath);
    return nullptr;
}
#endif

std::unique_ptr<TriangleMesh> MeshIO::loadOBJ(const std::string& filepath) {
    auto mesh = std::make_unique<TriangleMesh>();
    
    std::ifstream file(filepath);
    if (!file.is_open()) {
        spdlog::error("无法打开文件: {}", filepath);
        return nullptr;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string type;
        iss >> type;
        
        if (type == "v") {
            Vertex v{};
            iss >> v.x >> v.y >> v.z;
            v.nx = v.ny = v.nz = 0.0;
            mesh->vertices.push_back(v);
        } else if (type == "vn") {
            if (!mesh->vertices.empty()) {
                auto& v = mesh->vertices.back();
                iss >> v.nx >> v.ny >> v.nz;
            }
        } else if (type == "f") {
            Triangle tri{};
            std::string v0, v1, v2;
            iss >> v0 >> v1 >> v2;
            
            // 解析顶点索引（OBJ格式从1开始）
            tri.v0 = std::stoul(v0.substr(0, v0.find('/'))) - 1;
            tri.v1 = std::stoul(v1.substr(0, v1.find('/'))) - 1;
            tri.v2 = std::stoul(v2.substr(0, v2.find('/'))) - 1;
            
            mesh->triangles.push_back(tri);
        }
    }
    
    file.close();
    spdlog::info("加载OBJ文件成功: {} 顶点, {} 三角面", 
                 mesh->vertices.size(), mesh->triangles.size());
    return mesh;
}

std::unique_ptr<TriangleMesh> MeshIO::loadSTL(const std::string& filepath) {
    auto mesh = std::make_unique<TriangleMesh>();
    
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        spdlog::error("无法打开STL文件: {}", filepath);
        return nullptr;
    }
    
    // 读取文件头（80字节）
    char header[80];
    file.read(header, 80);
    
    // 读取三角形数量（4字节）
    uint32_t num_triangles = 0;
    file.read(reinterpret_cast<char*>(&num_triangles), sizeof(uint32_t));
    
    // 检查是否为ASCII格式（如果前5个字符是"solid"，可能是ASCII格式）
    file.seekg(0);
    std::string first_line;
    std::getline(file, first_line);
    file.seekg(0);
    
    bool is_ascii = false;
    if (first_line.find("solid") == 0 || first_line.find("SOLID") == 0) {
        // 尝试ASCII格式
        is_ascii = true;
        file.close();
        file.open(filepath, std::ios::in);
        
        std::string line;
        std::string token;
        std::map<std::string, size_t> vertex_map;  // 用于去重顶点
        size_t vertex_index = 0;
        
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            iss >> token;
            
            if (token == "vertex") {
                double x, y, z;
                iss >> x >> y >> z;
                
                // 创建顶点键用于去重
                std::ostringstream vertex_key;
                vertex_key << std::fixed << std::setprecision(6) << x << "," << y << "," << z;
                std::string key = vertex_key.str();
                
                // 检查顶点是否已存在
                auto it = vertex_map.find(key);
                size_t idx;
                if (it == vertex_map.end()) {
                    Vertex v{};
                    v.x = x;
                    v.y = y;
                    v.z = z;
                    v.nx = v.ny = v.nz = 0.0;
                    mesh->vertices.push_back(v);
                    vertex_map[key] = vertex_index;
                    idx = vertex_index;
                    vertex_index++;
                } else {
                    idx = it->second;
                }
                
                // 如果当前三角形未完成，添加顶点索引
                const size_t INVALID_INDEX = static_cast<size_t>(-1);
                if (mesh->triangles.empty() || 
                    mesh->triangles.back().v2 != INVALID_INDEX) {
                    // 开始新三角形
                    Triangle tri{};
                    tri.v0 = idx;
                    tri.v1 = INVALID_INDEX;
                    tri.v2 = INVALID_INDEX;
                    mesh->triangles.push_back(tri);
                } else if (mesh->triangles.back().v1 == INVALID_INDEX) {
                    mesh->triangles.back().v1 = idx;
                } else if (mesh->triangles.back().v2 == INVALID_INDEX) {
                    mesh->triangles.back().v2 = idx;
                }
            } else if (token == "facet" && !mesh->triangles.empty()) {
                // 读取法向量
                std::getline(file, line);
                std::istringstream iss2(line);
                std::string normal_token;
                iss2 >> normal_token;
                if (normal_token == "normal") {
                    double nx, ny, nz;
                    iss2 >> nx >> ny >> nz;
                    // 将法向量应用到当前三角形的顶点
                    if (!mesh->triangles.empty()) {
                        auto& tri = mesh->triangles.back();
                        if (tri.v0 != SIZE_MAX && tri.v0 < mesh->vertices.size()) {
                            mesh->vertices[tri.v0].nx = nx;
                            mesh->vertices[tri.v0].ny = ny;
                            mesh->vertices[tri.v0].nz = nz;
                        }
                    }
                }
            }
        }
        
        // 清理无效三角形
        const size_t INVALID_INDEX = static_cast<size_t>(-1);
        mesh->triangles.erase(
            std::remove_if(mesh->triangles.begin(), mesh->triangles.end(),
                [INVALID_INDEX](const Triangle& t) {
                    return t.v0 == INVALID_INDEX || t.v1 == INVALID_INDEX || t.v2 == INVALID_INDEX;
                }),
            mesh->triangles.end()
        );
        
    } else {
        // 二进制格式
        file.seekg(80);  // 跳过文件头
        file.read(reinterpret_cast<char*>(&num_triangles), sizeof(uint32_t));
        
        // 用于顶点去重的映射（坐标 -> 索引）
        std::map<std::string, size_t> vertex_map;
        size_t vertex_index = 0;
        const double EPSILON = 1e-6;
        
        // 读取每个三角形（50字节：12字节法向量 + 3*12字节顶点 + 2字节属性）
        for (uint32_t i = 0; i < num_triangles; ++i) {
            // 读取法向量（12字节，3个float）
            float nx, ny, nz;
            file.read(reinterpret_cast<char*>(&nx), sizeof(float));
            file.read(reinterpret_cast<char*>(&ny), sizeof(float));
            file.read(reinterpret_cast<char*>(&nz), sizeof(float));
            
            // 读取三个顶点（每个12字节，3个float）
            std::vector<size_t> vertex_indices;
            for (int j = 0; j < 3; ++j) {
                float x, y, z;
                file.read(reinterpret_cast<char*>(&x), sizeof(float));
                file.read(reinterpret_cast<char*>(&y), sizeof(float));
                file.read(reinterpret_cast<char*>(&z), sizeof(float));
                
                // 创建顶点键用于去重（使用与ASCII格式相同的精度）
                std::ostringstream vertex_key;
                vertex_key << std::fixed << std::setprecision(6) << x << "," << y << "," << z;
                std::string key = vertex_key.str();
                
                // 检查顶点是否已存在
                auto it = vertex_map.find(key);
                size_t idx;
                if (it == vertex_map.end()) {
                    // 新顶点
                    Vertex v{};
                    v.x = x;
                    v.y = y;
                    v.z = z;
                    v.nx = nx;
                    v.ny = ny;
                    v.nz = nz;
                    mesh->vertices.push_back(v);
                    vertex_map[key] = vertex_index;
                    idx = vertex_index;
                    vertex_index++;
                } else {
                    // 使用已存在的顶点索引
                    idx = it->second;
                    // 更新法向量（取平均值或使用当前值，这里简单使用当前值）
                    // 注意：STL格式中每个三角形有自己的法向量，这里简化处理
                }
                
                vertex_indices.push_back(idx);
            }
            
            // 添加三角形
            Triangle tri{};
            tri.v0 = vertex_indices[0];
            tri.v1 = vertex_indices[1];
            tri.v2 = vertex_indices[2];
            mesh->triangles.push_back(tri);
            
            // 跳过属性字节（2字节）
            uint16_t attribute;
            file.read(reinterpret_cast<char*>(&attribute), sizeof(uint16_t));
        }
    }
    
    file.close();
    
    if (mesh->vertices.empty() || mesh->triangles.empty()) {
        spdlog::error("STL文件为空或格式错误: {}", filepath);
        return nullptr;
    }
    
    spdlog::info("加载STL文件成功: {} 顶点, {} 三角面", 
                 mesh->vertices.size(), mesh->triangles.size());
    return mesh;
}

bool MeshIO::saveOBJ(const TriangleMesh& mesh, const std::string& filepath) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        spdlog::error("无法创建文件: {}", filepath);
        return false;
    }
    
    // 写入顶点
    for (const auto& v : mesh.vertices) {
        file << "v " << v.x << " " << v.y << " " << v.z << "\n";
        if (v.nx != 0 || v.ny != 0 || v.nz != 0) {
            file << "vn " << v.nx << " " << v.ny << " " << v.nz << "\n";
        }
    }
    
    // 写入面
    for (const auto& tri : mesh.triangles) {
        file << "f " << (tri.v0 + 1) << " " << (tri.v1 + 1) << " " << (tri.v2 + 1) << "\n";
    }
    
    file.close();
    return true;
}

bool MeshIO::saveSTL(const TriangleMesh& mesh, const std::string& filepath) {
    if (!mesh.isValid()) {
        spdlog::error("保存STL失败：网格无效");
        return false;
    }

    std::ofstream file(filepath, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        spdlog::error("无法创建STL文件: {}", filepath);
        return false;
    }

    char header[80] = {0};
    const std::string header_text = "NBCAM binary STL export";
    const size_t copy_len = std::min(header_text.size(), sizeof(header));
    std::copy(header_text.begin(), header_text.begin() + copy_len, header);
    file.write(header, sizeof(header));

    const uint32_t triangle_count = static_cast<uint32_t>(mesh.triangles.size());
    file.write(reinterpret_cast<const char*>(&triangle_count), sizeof(triangle_count));

    for (const auto& tri : mesh.triangles) {
        if (tri.v0 >= mesh.vertices.size() || tri.v1 >= mesh.vertices.size() || tri.v2 >= mesh.vertices.size()) {
            continue;
        }

        const auto& v0 = mesh.vertices[tri.v0];
        const auto& v1 = mesh.vertices[tri.v1];
        const auto& v2 = mesh.vertices[tri.v2];

        const float e1x = static_cast<float>(v1.x - v0.x);
        const float e1y = static_cast<float>(v1.y - v0.y);
        const float e1z = static_cast<float>(v1.z - v0.z);
        const float e2x = static_cast<float>(v2.x - v0.x);
        const float e2y = static_cast<float>(v2.y - v0.y);
        const float e2z = static_cast<float>(v2.z - v0.z);

        float nx = e1y * e2z - e1z * e2y;
        float ny = e1z * e2x - e1x * e2z;
        float nz = e1x * e2y - e1y * e2x;
        const float nlen = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (nlen > 1e-12f) {
            nx /= nlen;
            ny /= nlen;
            nz /= nlen;
        } else {
            nx = 0.0f;
            ny = 0.0f;
            nz = 0.0f;
        }

        const float normal[3] = {nx, ny, nz};
        file.write(reinterpret_cast<const char*>(normal), sizeof(normal));

        const float p0[3] = {static_cast<float>(v0.x), static_cast<float>(v0.y), static_cast<float>(v0.z)};
        const float p1[3] = {static_cast<float>(v1.x), static_cast<float>(v1.y), static_cast<float>(v1.z)};
        const float p2[3] = {static_cast<float>(v2.x), static_cast<float>(v2.y), static_cast<float>(v2.z)};
        file.write(reinterpret_cast<const char*>(p0), sizeof(p0));
        file.write(reinterpret_cast<const char*>(p1), sizeof(p1));
        file.write(reinterpret_cast<const char*>(p2), sizeof(p2));

        const uint16_t attr = 0;
        file.write(reinterpret_cast<const char*>(&attr), sizeof(attr));
    }

    file.close();
    const bool ok = file.good();
    if (!ok) {
        spdlog::error("保存STL失败（写入异常）: {}", filepath);
        return false;
    }

    spdlog::info("保存STL成功: {} (triangles={})", filepath, mesh.triangles.size());
    return true;
}

} // namespace nbcam
