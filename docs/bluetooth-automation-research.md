# ThinCube macOS 蓝牙配对确认自动化调研

调研日期：2026-10-11。本机运行 macOS 15.7.9（24G830），使用 Apple Swift 6.2.4 / macOS 26.2 SDK；产品最低部署目标为 macOS 13。用户描述的窗口是带设备名称的“配对 / 连接 / 允许”确认，没有六位码输入。本文区分系统蓝牙配对、应用蓝牙权限及 ThinCube 应用授权，记录公开 API 的支持范围和只读探针事实；没有执行自动批准系统窗口的试验。

## 结论与边界

正式产品目标推荐为：**已有有效系统绑定及 ThinCube 应用授权时自动重连，首次或系统忽略后清楚引导一次系统确认，随后立即进入醒目的 ThinCube 配对码窗口。** 当前资料与 SDK 检查没有找到可直接接管现有 CoreBluetooth 流程并自动接受无数字系统配对确认的公开 API。这个结论限定于已检查的公开接口，不能扩张为“任何 macOS 版本、任何办法都绝对不可能”。[Apple：Peripheral Connection Options](https://developer.apple.com/documentation/corebluetooth/peripheral-connection-options)

当前固件在 `ble_link.cpp` 设置 `ESP_IO_CAP_NONE` 与 `ESP_LE_AUTH_REQ_SC_BOND`，控制写入、通知订阅及音频特征要求加密。这是 NoInputNoOutput / Secure Connections bonding 路径：避免让用户在系统窗口输入蓝牙 PIN，并保存链路密钥供重连使用。**不需要蓝牙 PIN 输入，不等于操作系统不需要用户同意配对。** 最终系统确认行为仍需在首次绑定、系统忽略、密钥失效等情况下真机验证。Apple 的 Core Bluetooth 指南也区分连接和访问加密特征触发的配对过程。[本地固件实现](../firmware/src/ble_link.cpp)、[Apple：访问加密特征触发配对](https://developer.apple.com/library/archive/documentation/NetworkingInternetWeb/Conceptual/CoreBluetooth_concepts/BestPracticesForSettingUpYourIOSDeviceAsAPeripheral/BestPracticesForSettingUpYourIOSDeviceAsAPeripheral.html)

不推荐用辅助功能或 AppleScript 自动点击系统窗口作为常规配对机制。这需要额外的辅助功能信任，依赖窗口、语言和系统版本行为，并可能覆盖用户本次取消配对的选择。这是产品与维护成本的判断；本轮没有把“能够读到窗口”当作“能够稳定自动批准”的证据。[Apple：AXIsProcessTrustedWithOptions](https://developer.apple.com/documentation/applicationservices/1459186-axisprocesstrustedwithoptions)

## 公开 API 核对

| 接口 | 官方支持的行为 | 本项目结论 |
| --- | --- | --- |
| `CBCentralManager.connect(_:options:)` | 建立与外围设备的本地连接，连接结果通过 delegate 返回 | 自动调用连接可行，不代表自动接受系统配对 |
| `CBConnectPeripheralOptionNotifyOnConnectionKey` 等 | 后台连接、断开、通知时的系统提示；连接通知默认 `false` | 这些选项不是配对同意开关，设为 `false` 不能据此宣称关闭 bonding 确认 |
| `CBConnectPeripheralOptionEnableAutoReconnect` | 已连接的链路丢失后，系统自动发起重连 | SDK 标注 macOS 14+；macOS 13 保留应用重连逻辑。它不负责首次绑定或批准用户确认 |
| `IOBluetoothDevicePair` | 在应用内发起配对，可替代 IOBluetoothUI 的标准选择面板 | 初始化对象需要 `IOBluetoothDevice`，不是 `CBPeripheral`；尚无可靠公开映射及接管当前系统窗口的验证 |
| `replyUserConfirmation(_:)` | 回复 delegate 的数字比较确认，参数定义为用户提供的 yes/no | 方法存在不等于可自动批准 CoreBluetooth 加密 GATT 触发的无数字 Just Works 确认 |
| `CBManager.authorization` / `NSBluetoothAlwaysUsageDescription` | 查询应用访问权限，系统向用户询问应用能否使用蓝牙 | TCC 权限与设备 bonding 分开；普通产品代码不能通过 CoreBluetooth 为自己授予权限 |
| `AccessorySetupKit` | iOS 的附件发现、授权和设置流程 | 本机 SDK 的 `ASAccessorySession` 等明确 `API_UNAVAILABLE(macos, macCatalyst)`；不能把 iOS 的一次点击方案直接套到本机应用 |

表中行为依据：[Apple：connect](https://developer.apple.com/documentation/corebluetooth/cbcentralmanager/connect(_:options:))、[后台连接通知](https://developer.apple.com/documentation/corebluetooth/cbconnectperipheraloptionnotifyonconnectionkey)、[自动重连](https://developer.apple.com/documentation/corebluetooth/cbconnectperipheraloptionenableautoreconnect)、[IOBluetoothDevicePair](https://developer.apple.com/documentation/iobluetooth/iobluetoothdevicepair)、[replyUserConfirmation](https://developer.apple.com/documentation/iobluetooth/iobluetoothdevicepair/replyuserconfirmation(_:))、[受保护资源与权限](https://developer.apple.com/documentation/bundleresources/protected-resources)、[AccessorySetupKit](https://developer.apple.com/documentation/accessorysetupkit)。最低版本和不可用声明来自本机 Apple SDK 的 `CBCentralManagerConstants.h` 与 `ASAccessorySession.h`，SDK 版本不作为运行版本。

不能简单把 `IOBluetoothDevicePair` 归为“只支持 Classic”。当前官方类关系和 SDK 声明都显示它遵循 `CBCentralManagerDelegate`。但这一事实本身也不保证它能接管 ThinCube 的 BLE-only 流程；其确认 delegate 的说明是 Secure Simple Pairing 数字比较，而非任意系统窗口的自动同意接口。[Apple：配对类关系](https://developer.apple.com/documentation/iobluetooth/iobluetoothdevicepair)、[确认 delegate](https://developer.apple.com/documentation/iobluetooth/iobluetoothdevicepairdelegate/devicepairinguserconfirmationrequest(_:numericvalue:))

## 本机只读可行性探针

独立探针及记录放在被 Git 忽略的 `artifacts/connection-pairing-v0.4.6/`。探针先查询已有 `CBManager.authorization`；未授权即退出，不实例化 central 或请求权限。授权已存在时，仅枚举已配对 / 最近设备、检索当前连接的指定服务并扫描八秒。只创建未启动的 `IOBluetoothDevicePair` 对象，检查公开确认方法的存在；从未调用连接、断开、配对 `start`、确认回复、远端名称请求、SDP 或修改设置。

- `BluetoothMappingProbe.swift` 使用 `arm64-apple-macosx13.0` 目标编译通过，证明所使用接口可在最低部署目标编译；没有在 macOS 13 实机运行。
- 沙箱内运行的授权值为 `denied`，按保护条件退出；沙箱外运行上下文已经是 `allowedAlways`，没有新增权限窗口。系统版本确认为 15.7.9，central 为 `poweredOn`。
- `IOBluetoothDevice.pairedDevices()` 枚举到 19 个系统记录，其中一个名为 `ThinCube-6046E5`，`isPaired=true`。初次运行时 `isConnected=false`；主应用通过 UI 切换到蓝牙并保持连接后，重跑得到 `isConnected=true`。对象可分配未启动的 `IOBluetoothDevicePair`，目标对象相同，`replyUserConfirmation(_:)` 方法存在。为避免记录其他用户设备，探针只保存非 ThinCube 设备的数量。
- 首次八秒扫描及 `retrieveConnectedPeripherals(withServices:)` 没有返回 ThinCube；主应用使用固件 0.4.10 / 上位机 0.4.6 保持蓝牙连接后，公开检索返回名为 `ThinCube-6046E5` 的 peer，UUID 为 `4E79B467-A615-B51A-04E4-D73327B3C3CF`，同名 IOBluetooth 候选地址为 `7c-e8-b1-e5-46-61`。这支持“当前两个框架都能读到同名对象”的事实，仍不是公开的 UUID→BD_ADDR 身份桥接；扫描未新增此对象。
- 把该 UUID 字符串直接传给 `IOBluetoothDevice.init(addressString:)` 意外返回非空对象，但其实际地址是 `4e-79-b4-67-a6-15`，与系统 ThinCube 候选不符，也不是同一对象。构造函数对字符串的宽松解析不能作为映射成功的证据；探针没有尝试连接这个地址。
- `CBPeer.identifier` 是系统分配的 UUID；`IOBluetoothDevice.init(addressString:)` 要求 `xx:xx:xx:xx:xx:xx` 格式的 BD_ADDR。公开资料没有给出 UUID 转 BD_ADDR 的桥接。按名称匹配只能得到未验证候选，对首次尚未绑定的设备还可能没有系统记录可枚举。[Apple：CBPeer.identifier](https://developer.apple.com/documentation/corebluetooth/cbpeer/identifier)、[IOBluetooth 地址初始化](https://developer.apple.com/documentation/iobluetooth/iobluetoothdevice/init(addressstring:))、[系统已配对设备枚举](https://developer.apple.com/documentation/iobluetooth/iobluetoothdevice/paireddevices())

证据：首次运行的 `bluetooth-mapping-probe-sandbox.json`、`bluetooth-mapping-probe.json`，以及已有蓝牙连接下的 `bluetooth-mapping-probe-connected.json`、`bluetooth-mapping-probe-connected-v2.json`。本轮支持的事实是“公开 API 编译通过、已有系统对象和当前 peer 可检索、配对对象可无副作用分配”；未支持的事实是“身份可靠映射、首次 BLE 配对可被 IOBluetooth 接管、确认 callback 确实到达、系统窗口可免点击”。

## 实施取舍与后续验收

- 生产流程继续用 CoreBluetooth；保留加密特征和系统密钥复用，避免为了消除窗口而降低链路保护。正常重连的目标是零点击；系统或设备已撤销绑定时进入可恢复的确认引导。
- 上位机将阶段写清楚：信道连接中 → 必要时“请在系统窗口允许与此设备配对” → 检查 ThinCube 授权 → 必要时弹出六位 `PAIR PIN` 输入。用户拒绝 / 取消系统确认或应用授权后停止本次自动重试。
- 若继续探索 IOBluetooth，需要先获得可验证且适用于首次未绑定设备的身份关系，不能只按名字或截取 UUID 构造地址。再由根代理协调单独时段决定是否测试配对 delegate。配对测试会改变绑定状态，本轮未执行，也不在正式交付中承诺该能力。
- 记录首次无绑定、有效绑定重连、系统忽略后、设备端撤销授权、取消确认、密钥失效和设备重启各自的系统 / 应用窗口次数；当前结果只代表 macOS 15.7.9，macOS 13 与后续版本仍需实机覆盖。
