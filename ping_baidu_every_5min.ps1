while ($true) {
    $time = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $result = Test-Connection -ComputerName "www.baidu.com" -Count 1 -Quiet
    if ($result) {
        Write-Host "$time 百度可达"
    } else {
        Write-Host "$time 百度不可达"
    }
    Start-Sleep -Seconds 300
}
