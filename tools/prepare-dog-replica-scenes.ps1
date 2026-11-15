param([string]$AnalysisRoot='D:\mgsv-analysis\mgsv-analysis')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$source=Join-Path $AnalysisRoot 'coop-investigation/buddies/scenes'
$out=Join-Path $root 'moddata/dog_replicas'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$manifest=Get-Content -Raw -LiteralPath (Join-Path $source 'manifest.json') | ConvertFrom-Json
foreach($entry in $manifest) {
    if($entry.path -notmatch '/dog/buddy_dog2_(0[0-4])\.fox2$') { continue }
    $variant=$Matches[1]
    [xml]$xml=Get-Content -Raw -LiteralPath (Join-Path $source ($entry.sha256+'.fox2.xml'))
    $entities=$xml.fox.entities
    $pool=$entities.SelectSingleNode('entity[@class="GameObject"]')
    foreach($name in @('totalCount','realizedCount')) { $pool.SelectSingleNode("staticProperties/property[@name='$name']/value").InnerText='2' }
    $locator=$entities.SelectSingleNode('entity[@class="GameObjectLocator"]')
    $transform=$entities.SelectSingleNode('entity[@class="TransformEntity"]')
    $parameter=$entities.SelectSingleNode('entity[@class="TppBuddyDog2LocatorParameter"]')
    if(-not $locator -or -not $transform -or -not $parameter){throw 'Incomplete Dog locator graph'}
    $newLocator=$locator.CloneNode($true)
    $newTransform=$transform.CloneNode($true)
    $newParameter=$parameter.CloneNode($true)
    $newLocator.SetAttribute('addr','0x7F000000')
    $newTransform.SetAttribute('addr','0x7F000100')
    $newParameter.SetAttribute('addr','0x7F000200')
    $newLocator.SetAttribute('unknown2','8323072')
    $newTransform.SetAttribute('unknown2','8323073')
    $newParameter.SetAttribute('unknown2','8323074')
    $newLocator.SelectSingleNode('staticProperties/property[@name="name"]/value').InnerText='BuddyDogGameObjectLocator_Remote'
    $newLocator.SelectSingleNode('staticProperties/property[@name="transform"]/value').InnerText='0x7F000100'
    $newLocator.SelectSingleNode('staticProperties/property[@name="parameters"]/value').InnerText='0x7F000200'
    foreach($node in @($newTransform,$newParameter)) { $node.SelectSingleNode('staticProperties/property[@name="owner"]/value').InnerText='0x7F000000' }
    $newParameter.SelectSingleNode('staticProperties/property[@name="identifier"]/value').InnerText='DynamiteRemoteDog'
    foreach($node in @($newLocator,$newTransform,$newParameter)) { $null=$entities.AppendChild($node) }
    $list=$entities.SelectSingleNode('entity[@class="DataSet"]/staticProperties/property[@name="dataList"]')
    $value=$xml.CreateElement('value'); $value.SetAttribute('key','BuddyDogGameObjectLocator_Remote'); $value.InnerText='0x7F000000'
    $null=$list.AppendChild($value); $list.SetAttribute('arraySize',([int]$list.GetAttribute('arraySize')+1).ToString())
    $xml.Save((Join-Path $out ('buddy_dog2_'+$variant+'.fox2.xml')))
    & (Join-Path $AnalysisRoot 'coop-investigation/buddies/fox-bin/FoxTool.exe') -c (Join-Path $out ('buddy_dog2_'+$variant+'.fox2.xml'))
    if($LASTEXITCODE -ne 0){throw 'FoxTool scene compilation failed'}
}
$go=Join-Path $root '.build-tools/go/go/bin/go.exe'
if(-not (Test-Path -LiteralPath $go)){throw 'The repository Go toolchain is required to import the FoxTool scenes'}
$env:GOPATH=Join-Path $root '.build-tools/gopath'
$env:GOCACHE=Join-Path $root '.build-tools/gocache'
Push-Location -LiteralPath $root
try {
    & $go run moddata/dog_replicas/generate.go -import
    if($LASTEXITCODE -ne 0){throw 'Import into the regular Go scene generator failed'}
} finally { Pop-Location }
