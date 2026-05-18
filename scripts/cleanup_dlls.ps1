# 清理 build/bin/Release 中未使用的 DLL
# 根据 CMakeLists.txt，项目只使用以下 VTK 组件：
# - VTK::CommonCore
# - VTK::RenderingCore
# - VTK::RenderingOpenGL2
# - VTK::InteractionStyle
# - VTK::GUISupportQt

$ReleaseDir = "build\bin\Release"

if (-not (Test-Path $ReleaseDir)) {
    Write-Host "错误: 找不到目录 $ReleaseDir" -ForegroundColor Red
    exit 1
}

Write-Host "正在分析 DLL 文件..." -ForegroundColor Yellow

# 基于实际使用的 VTK 组件，这些是最小必需的 DLL 集合
# 这些 DLL 是 VTK::CommonCore, RenderingCore, RenderingOpenGL2, InteractionStyle, GUISupportQt 的依赖
$RequiredVTKDLLs = @(
    # CommonCore 依赖
    "vtkCommonCore-9.4.dll",
    "vtkCommonDataModel-9.4.dll",
    "vtkCommonExecutionModel-9.4.dll",
    "vtkCommonMath-9.4.dll",
    "vtkCommonMisc-9.4.dll",
    "vtkCommonSystem-9.4.dll",
    "vtkCommonTransforms-9.4.dll",
    "vtksys-9.4.dll",
    "vtktoken-9.4.dll",
    
    # RenderingCore 依赖
    "vtkRenderingCore-9.4.dll",
    "vtkRenderingOpenGL2-9.4.dll",
    "vtkRenderingFreeType-9.4.dll",
    "vtkRenderingUI-9.4.dll",
    "vtkRenderingContext2D-9.4.dll",
    "vtkRenderingContextOpenGL2-9.4.dll",
    "vtkRenderingLabel-9.4.dll",
    "vtkRenderingLOD-9.4.dll",
    "vtkRenderingSceneGraph-9.4.dll",
    "vtkRenderingAnnotation-9.4.dll",
    "vtkRenderingImage-9.4.dll",
    "vtkRenderingVolume-9.4.dll",
    "vtkRenderingVolumeOpenGL2-9.4.dll",
    
    # GUISupportQt 依赖
    "vtkGUISupportQt-9.4.dll",
    "vtkRenderingQt-9.4.dll",
    "vtkViewsQt-9.4.dll",
    "vtkViewsCore-9.4.dll",
    "vtkViewsContext2D-9.4.dll",
    
    # InteractionStyle 依赖
    "vtkInteractionStyle-9.4.dll",
    "vtkInteractionWidgets-9.4.dll",
    "vtkInteractionImage-9.4.dll",
    
    # Filters (被 RenderingCore 使用)
    "vtkFiltersCore-9.4.dll",
    "vtkFiltersGeneral-9.4.dll",
    "vtkFiltersGeometry-9.4.dll",
    "vtkFiltersSources-9.4.dll",
    "vtkFiltersExtraction-9.4.dll",
    "vtkFiltersStatistics-9.4.dll",
    "vtkFiltersHybrid-9.4.dll",
    "vtkFiltersModeling-9.4.dll",
    "vtkFiltersPoints-9.4.dll",
    "vtkFiltersProgrammable-9.4.dll",
    "vtkFiltersSMP-9.4.dll",
    "vtkFiltersTexture-9.4.dll",
    "vtkFiltersTopology-9.4.dll",
    "vtkFiltersVerdict-9.4.dll",
    
    # IO (被 Filters 使用)
    "vtkIOCore-9.4.dll",
    "vtkIOGeometry-9.4.dll",
    "vtkIOLegacy-9.4.dll",
    "vtkIOPLY-9.4.dll",
    "vtkIOImage-9.4.dll",
    "vtkIOImport-9.4.dll",
    "vtkIOExport-9.4.dll",
    "vtkIOXML-9.4.dll",
    "vtkIOXMLParser-9.4.dll",
    
    # Imaging (被 Filters 使用)
    "vtkImagingCore-9.4.dll",
    "vtkImagingSources-9.4.dll",
    "vtkImagingGeneral-9.4.dll",
    "vtkImagingHybrid-9.4.dll",
    "vtkImagingMath-9.4.dll",
    "vtkImagingMorphological-9.4.dll",
    "vtkImagingStatistics-9.4.dll",
    "vtkImagingStencil-9.4.dll",
    "vtkImagingFourier-9.4.dll",
    
    # Parallel (被 Filters 使用)
    "vtkParallelCore-9.4.dll",
    "vtkParallelDIY-9.4.dll",
    
    # Infovis (被 Views 使用)
    "vtkInfovisCore-9.4.dll",
    "vtkInfovisLayout-9.4.dll",
    "vtkViewsInfovis-9.4.dll",
    
    # 第三方库
    "vtkzlib-9.4.dll",
    "vtkpng-9.4.dll",
    "vtkjpeg-9.4.dll",
    "vtktiff-9.4.dll",
    "vtkjsoncpp-9.4.dll",
    "vtklibxml2-9.4.dll",
    "vtkpugixml-9.4.dll",
    "vtkmetaio-9.4.dll",
    "vtkloguru-9.4.dll",
    "vtklz4-9.4.dll",
    "vtklzma-9.4.dll",
    "vtkkissfft-9.4.dll",
    "vtkverdict-9.4.dll",
    "vtkfreetype-9.4.dll",
    "vtkglad-9.4.dll",
    "vtkdoubleconversion-9.4.dll",
    "vtkfmt-9.4.dll",
    "zlib1.dll"
)

# 获取所有 VTK DLL 文件
$AllVTKDLLs = Get-ChildItem -Path $ReleaseDir -Filter "vtk*.dll" -File

Write-Host "找到 $($AllVTKDLLs.Count) 个 VTK DLL 文件" -ForegroundColor Cyan

# 找出不需要的 VTK DLL
$UnusedVTKDLLs = @()
foreach ($dll in $AllVTKDLLs) {
    $dllName = $dll.Name
    if ($dllName -notin $RequiredVTKDLLs) {
        $UnusedVTKDLLs += $dll
    }
}

Write-Host "发现 $($UnusedVTKDLLs.Count) 个未使用的 VTK DLL" -ForegroundColor Yellow

if ($UnusedVTKDLLs.Count -eq 0) {
    Write-Host "没有需要清理的 VTK DLL" -ForegroundColor Green
    exit 0
}

# 显示将要删除的 DLL
Write-Host "`n将要删除以下 VTK DLL:" -ForegroundColor Yellow
foreach ($dll in $UnusedVTKDLLs) {
    Write-Host "  - $($dll.Name)" -ForegroundColor Gray
}

# 确认删除
$confirmation = Read-Host "`n确认删除这些 DLL? (Y/N)"
if ($confirmation -ne "Y" -and $confirmation -ne "y") {
    Write-Host "操作已取消" -ForegroundColor Yellow
    exit 0
}

# 删除 DLL
$deletedCount = 0
foreach ($dll in $UnusedVTKDLLs) {
    try {
        Remove-Item -Path $dll.FullName -Force
        Write-Host "已删除: $($dll.Name)" -ForegroundColor Green
        $deletedCount++
    } catch {
        Write-Host "删除失败: $($dll.Name) - $($_.Exception.Message)" -ForegroundColor Red
    }
}

Write-Host "`n清理完成! 删除了 $deletedCount 个 VTK DLL" -ForegroundColor Green
Write-Host "保留的 VTK DLL: $($RequiredVTKDLLs.Count) 个" -ForegroundColor Cyan
