$ErrorActionPreference = "Stop"

$root = "E:\haohaohao"
$docs = Join-Path $root "docs"
$output = Join-Path $docs "a_visio_flowchart.vsdx"
$stencil = "C:\Program Files\Microsoft Office\root\Office16\Visio Content\2052\BASFLO_M.VSSX"

$visio = New-Object -ComObject Visio.Application
$visio.Visible = $false
$visio.AlertResponse = 7

$doc = $visio.Documents.Add("")
$stDoc = $visio.Documents.OpenEx($stencil, 64)
$page = $visio.ActivePage
$page.Name = "A流程图"
$page.PageSheet.CellsU("PageWidth").ResultIU = 44
$page.PageSheet.CellsU("PageHeight").ResultIU = 12

function Add-Shape([object]$stencilDoc, [object]$targetPage, [string]$masterName, [double]$x, [double]$y, [string]$text) {
    $m = $stencilDoc.Masters.ItemU($masterName)
    $s = $targetPage.Drop($m, $x, $y)
    $s.Text = $text
    return $s
}

function Connect-Shape([object]$stencilDoc, [object]$targetPage, [object]$fromShape, [object]$toShape, [string]$label = "") {
    $m = $stencilDoc.Masters.ItemU("Dynamic connector")
    $c = $targetPage.Drop($m, 0, 0)
    $c.CellsU("BeginX").GlueTo($fromShape.CellsU("PinX"))
    $c.CellsU("EndX").GlueTo($toShape.CellsU("PinX"))
    if ($label -ne "") {
        $c.Text = $label
    }
    return $c
}

$yMid = 6.0
$yTop = 8.3
$yBottom = 3.7

$s1 = Add-Shape $stDoc $page "Start/End" 1.8 $yMid "开始"
$s2 = Add-Shape $stDoc $page "Process"   4.0 $yMid "导入工件模型`n(STL / OBJ)"
$d1 = Add-Shape $stDoc $page "Decision"  6.5 $yMid "工件曲面是否简单？"
$p3a = Add-Shape $stDoc $page "Process"  8.8 $yTop "二面角聚类`n设置阈值并提取Patch"
$p3b = Add-Shape $stDoc $page "Process"  8.8 $yBottom "手动设置加工平面`n提取平面以上待加工区域"
$p4 = Add-Shape $stDoc $page "Process"   11.5 $yMid "获得待加工曲面Patch"
$p5 = Add-Shape $stDoc $page "Process"   14.0 $yMid "参数化展开到UV平面"
$d2 = Add-Shape $stDoc $page "Decision"  16.5 $yMid "选择参数化算法"
$p6a = Add-Shape $stDoc $page "Process"  18.8 $yTop "LSCM`n保角优先 / 速度快"
$p6b = Add-Shape $stDoc $page "Process"  18.8 $yBottom "ARAP`n刚性优先 / 畸变更均匀"
$p7 = Add-Shape $stDoc $page "Process"   21.0 $yMid "在UV面片应用SVG纹理`n并进行纹理编辑"
$p8 = Add-Shape $stDoc $page "Process"   23.8 $yMid "按参数化映射回曲面`nAlpha裁剪得到路径边界"
$p9 = Add-Shape $stDoc $page "Process"   26.3 $yMid "设置加工与图案参数`n(间隔/角度/策略)"
$p10 = Add-Shape $stDoc $page "Process"  28.8 $yMid "生成路径`n(往返/回型/轮廓/等Z/弧线填充)"
$p11 = Add-Shape $stDoc $page "Process"  31.4 $yMid "3D/UV/路径预览联合检查`n(分段/开关光/方向)"
$d3 = Add-Shape $stDoc $page "Decision"  34.0 $yMid "结果是否确认无误？"
$p12 = Add-Shape $stDoc $page "Process"  36.8 $yBottom "调整参数或策略`n重新生成路径"
$p13 = Add-Shape $stDoc $page "Process"  36.8 $yTop "导出CSV/任务文件`n或直接下发执行"
$s14 = Add-Shape $stDoc $page "Start/End" 40.5 $yTop "结束"

Connect-Shape $stDoc $page $s1  $s2 | Out-Null
Connect-Shape $stDoc $page $s2  $d1 | Out-Null
Connect-Shape $stDoc $page $d1  $p3a "是" | Out-Null
Connect-Shape $stDoc $page $d1  $p3b "否" | Out-Null
Connect-Shape $stDoc $page $p3a $p4 | Out-Null
Connect-Shape $stDoc $page $p3b $p4 | Out-Null
Connect-Shape $stDoc $page $p4  $p5 | Out-Null
Connect-Shape $stDoc $page $p5  $d2 | Out-Null
Connect-Shape $stDoc $page $d2  $p6a "LSCM" | Out-Null
Connect-Shape $stDoc $page $d2  $p6b "ARAP" | Out-Null
Connect-Shape $stDoc $page $p6a $p7 | Out-Null
Connect-Shape $stDoc $page $p6b $p7 | Out-Null
Connect-Shape $stDoc $page $p7  $p8 | Out-Null
Connect-Shape $stDoc $page $p8  $p9 | Out-Null
Connect-Shape $stDoc $page $p9  $p10 | Out-Null
Connect-Shape $stDoc $page $p10 $p11 | Out-Null
Connect-Shape $stDoc $page $p11 $d3 | Out-Null
Connect-Shape $stDoc $page $d3  $p13 "是" | Out-Null
Connect-Shape $stDoc $page $d3  $p12 "否" | Out-Null
Connect-Shape $stDoc $page $p12 $p9 "调整后重试" | Out-Null
Connect-Shape $stDoc $page $p13 $s14 | Out-Null

$doc.SaveAs($output)

$stDoc.Close()
$doc.Close()
$visio.Quit()

[System.Runtime.Interopservices.Marshal]::ReleaseComObject($s14) | Out-Null
[System.Runtime.Interopservices.Marshal]::ReleaseComObject($p13) | Out-Null
[System.Runtime.Interopservices.Marshal]::ReleaseComObject($p12) | Out-Null
[System.Runtime.Interopservices.Marshal]::ReleaseComObject($d3) | Out-Null
[System.Runtime.Interopservices.Marshal]::ReleaseComObject($p11) | Out-Null
[System.Runtime.Interopservices.Marshal]::ReleaseComObject($p10) | Out-Null
[System.Runtime.Interopservices.Marshal]::ReleaseComObject($p9) | Out-Null
[System.Runtime.Interopservices.Marshal]::ReleaseComObject($p8) | Out-Null
[System.Runtime.Interopservices.Marshal]::ReleaseComObject($p7) | Out-Null
[System.Runtime.Interopservices.Marshal]::ReleaseComObject($p6b) | Out-Null
[System.Runtime.Interopservices.Marshal]::ReleaseComObject($p6a) | Out-Null
[System.Runtime.Interopservices.Marshal]::ReleaseComObject($d2) | Out-Null
[System.Runtime.Interopservices.Marshal]::ReleaseComObject($p5) | Out-Null
[System.Runtime.Interopservices.Marshal]::ReleaseComObject($p4) | Out-Null
[System.Runtime.Interopservices.Marshal]::ReleaseComObject($p3b) | Out-Null
[System.Runtime.Interopservices.Marshal]::ReleaseComObject($p3a) | Out-Null
[System.Runtime.Interopservices.Marshal]::ReleaseComObject($d1) | Out-Null
[System.Runtime.Interopservices.Marshal]::ReleaseComObject($s2) | Out-Null
[System.Runtime.Interopservices.Marshal]::ReleaseComObject($s1) | Out-Null
[System.Runtime.Interopservices.Marshal]::ReleaseComObject($page) | Out-Null
[System.Runtime.Interopservices.Marshal]::ReleaseComObject($stDoc) | Out-Null
[System.Runtime.Interopservices.Marshal]::ReleaseComObject($doc) | Out-Null
[System.Runtime.Interopservices.Marshal]::ReleaseComObject($visio) | Out-Null

Write-Output "SAVED=$output"
