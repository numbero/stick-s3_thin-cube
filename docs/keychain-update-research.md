# ThinCube 更新后反复要求钥匙串密码的调研

调研日期：2026-10-09。范围：截图中的 `app.thincube.device` 登录钥匙串授权。本文记录初始只读调查与后续用一次性测试凭据进行的签名验证；未读取或修改真实设备凭据。实施期间创建了本机自签身份并构建安装进行验证，后续结果修正了最初关于自签方案的判断。

## 结论

根因证据高度一致：ThinCube 会读取登录钥匙串中的设备凭据，但最初本机构建使用 ad hoc 签名。这种应用身份绑定具体内容的 `cdhash`；内容不同的新构建无法匹配旧构建的指定要求（Designated Requirement，DR），因而不能继承旧应用的钥匙串信任。路径始终为 `/Applications/ThinCube.app`、名称和 bundle identifier 不变，也不能代替签名身份连续性。Apple 官方解释 ad hoc 签名只标识一个具体程序版本，`cdhash` 随程序的实质性变化而改变；Apple 源码也直接为 ad hoc 签名生成 `cdhash` DR。[Code Signing Requirement Language](https://developer.apple.com/library/archive/documentation/Security/Conceptual/CodeSigningGuide/RequirementLang/RequirementLang.html)、[Apple Security：StaticCode.cpp](https://github.com/apple-oss-distributions/Security/blob/main/OSX/libsecurity_codesigning/lib/StaticCode.cpp)

**补充验证后的重要修正：固定本机自签证书只能稳定 DR，不能单独解决现代登录钥匙串跨构建免密。** 现代 file-based keychain 还独立检查 `partition_id`；普通自签应用仍被归类为 `cdhash:<当前版本hash>`。Apple 官方源码先进行普通 ACL 校验，再进行 partition 校验；Apple DTS 也确认这两个条件必须同时满足。因此完整修复需要稳定 DR 和稳定的钥匙串 partition，而不是只检查 `codesign` 的证书与 DR。[Apple Security：clientid.cpp](https://github.com/apple-oss-distributions/Security/blob/main/securityd/src/clientid.cpp)、[Apple Security：acls.cpp](https://github.com/apple-oss-distributions/Security/blob/main/securityd/src/acls.cpp)、[Apple DTS：File Keychain ACL + partition list](https://developer.apple.com/forums/thread/845980)

这张截图要求的是授权 ThinCube 读取特定钥匙串机密，并非替换 `/Applications` 应用时的管理员安装密码。`允许`只批准当次，`始终允许`把应用加入该项的信任列表；内容改变后的 ad hoc 应用仍可能被识别为另一身份。[Apple：Allow apps to access your keychain](https://support.apple.com/guide/mac-help/kychn002/mac)、[Apple：Access Control Lists](https://developer.apple.com/documentation/security/access-control-lists)

为何必须输入密码而不能只确认：Apple 的 `SecACLSetContents` 文档明确注明，自 macOS 10.13.1 起，系统忽略 ACL 的 `promptSelector`，在询问用户是否将应用加入可信应用列表时始终要求钥匙串密码。登录钥匙串已解锁，也不等于任意新应用有权读取其中的秘密。[SecACLSetContents](https://developer.apple.com/documentation/security/secaclsetcontents(_:_:_:_:))

## 初始本机证据

以下为主调查过程的只读检查结果：

- `scripts/build-macos.sh:25` 固定执行 `codesign --force --sign -`，即 ad hoc 签名。
- `macos/Sources/ThinCube/Credentials.swift:15` 起使用 `SecItem` generic password，service 为 `app.thincube.device`；创建和读取未指定 `kSecUseDataProtectionKeychain`，也未显式设置可信应用 ACL。
- 初始 `/Applications/ThinCube.app`：`Signature=adhoc`、`TeamIdentifier=not set`；identifier 为 `app.thincube.mac`，DR 为 `cdhash H"ade56c59fe4bc17a549be205cdde4583fb69ce31"`。
- 旧备份 `artifacts/install-backups/20261009-211702-40898/ThinCube.app` 的同一 identifier，DR 为 `cdhash H"accf1df2521cf8b5c6f45a1e6cf3936a54ce0a39"`。
- 两份应用自身签名均通过严格校验。用旧 DR 验证旧应用成功；用同一旧 DR 验证当前应用失败，退出码 `3`，输出 `code failed to satisfy specified code requirement(s)`。这是应用身份不连续的直接验证。
- `diff -qr artifacts/ThinCube.app /Applications/ThinCube.app` 返回 `0`，构建产物与系统应用内容一致，未发现只更新产物、系统应用未替换的问题。
- 默认钥匙串搜索范围执行 `security find-identity -v -p codesigning` 返回 `0 valid identities found`。这不能推导为所有位置都不存在证书。

Apple 文档说明 macOS 的 `SecItem` 默认使用传统 file-based keychain；只有显式选择才改为 data protection keychain。因此当前代码采用的默认路径与截图的登录钥匙串 ACL 提示一致。[TN3137：On Mac keychain APIs and implementations](https://developer.apple.com/documentation/technotes/tn3137-on-mac-keychains)

未检查实际凭据条目的 ACL 内容，不能声称已逐项证明截图条目保存了哪个旧 hash。可直接证明的是构建身份变化、旧 DR 校验失败、应用确实访问该 service，以及这些现象与 Apple 的授权机制相符。字节和签名内容完全相同的重新安装不必然产生新 hash；本结论针对内容改变的研发构建。

## 自签方案的实际验证与失败原因

实施验证创建了固定本机 Code Signing 证书（公开 SHA-1 指纹 `5AACC9E9CED03CF8ADBB68C4B4381D8659072CF0`），签名要求固定为：

```text
designated => identifier "app.thincube.mac" and certificate leaf = H"5aacc9e9ced03cf8adbb68c4b4381d8659072cf0"
```

两个内容不同的 Swift 测试 `.app` 使用相同 identifier、证书与 DR，均能满足对方的签名要求；完整 ThinCube 构建与安装也通过签名校验。但测试构建 1 通过 `SecItemAdd` 创建一次性 `app.thincube.signing-validation.<UUID>` generic password 后，构建 2 在禁止交互时读取仍返回 `-25293`（`errSecAuthFailed`）。将两个版本先后替换到同一 `.app` 路径再次验证，读取仍失败；排除了仅因测试路径不同导致失败的解释。原创建程序可清理其测试项；同路径更新程序的删除也成功，说明删除权限不能用来证明机密读取已授权。本实验不访问真实 `app.thincube.device` 凭据。

Apple 的 `ClientIdentification::partitionIdForProcess` 在 Apple/MAS/开发或分发证书分类未命中时，针对已签名程序返回 `cdhash:` 加代码目录 hash。`SecurityServerAcl::validatePartition` 精确匹配这个字符串；用户同意后 `extendPartition` 追加的是当前调用程序的 partition。因此自签新版本即使稳定 DR 已匹配，下一次内容变化仍会遇到新的 partition。删除 partition ACL 也不能持久规避：源码在它缺失时会自动为调用程序创建一个。[clientid.cpp](https://github.com/apple-oss-distributions/Security/blob/main/securityd/src/clientid.cpp)、[acls.cpp](https://github.com/apple-oss-distributions/Security/blob/main/securityd/src/acls.cpp)

稳定 team partition 的开发证书分类要求为：Apple generic anchor、WWDR 中间证书扩展 `1.2.840.113635.100.6.2.1`、leaf 扩展 `1.2.840.113635.100.6.1.12`；匹配后取 leaf subject 的 OU 作为 `teamid:`。统一的 **Apple Development** 证书并非遗漏：Apple WWDR CPS §4.11.23 明确规定它同时具有 iPhone 开发 `.1.2` 与 Mac 开发 `.1.12` 两个扩展，所以可以满足这一分类。Developer ID 则匹配中间证书 `.2.6` 与 leaf `.1.13`。仅给自签证书添加 OU、修改签名 TeamIdentifier 或赋予本机 codeSign 信任，不会形成 Apple generic anchor 链。[clientid.cpp](https://github.com/apple-oss-distributions/Security/blob/main/securityd/src/clientid.cpp)、[Apple WWDR CPS v1.32，§4.11.23](https://images.apple.com/certificateauthority/pdf/Apple_WWDR_CPS_v1.32.pdf)

## 为什么 folotoy 不出现

本机对照项目 `folotoy-vibecard`：`tools/package_macapp.sh` 可通过环境变量 `VIBECARD_SIGN_IDENTITY` 选择签名身份，默认也是 `-`；已安装 `/Applications/VibeCard.app` 同样为 ad hoc 签名。其 `macapp/Sources` 没有 `SecItem`、`SecKeychain` 或 Keychain API 调用；`Preferences.swift` 用 `UserDefaults` 保存偏好。当前对照版本未发现 ThinCube 这种逐设备持久保存和自动读取应用钥匙串配对凭据的路径。

因此主要差异是是否读取受 ACL 保护的钥匙串凭据，而非 folotoy 已经通过更高级的签名自动免密。此结论限定于检查到的本机项目版本，不代表所有 folotoy 分支或历史版本。

## 可行方案

| 方案 | 效果与代价 | 判断 |
| --- | --- | --- |
| 固定 Apple Development 或已有 Developer ID Application 身份 | 可同时建立稳定 DR 与 Apple 分类的 team partition；需可用证书和私钥、配置后做实际跨版本机密读取验证。第一次读旧凭据仍可能要求授权 | 当前首选 |
| 固定本机自签 Code Signing 证书，所有研发构建复用 | DR 可稳定，但 partition 仍是当前 `cdhash`；本机跨版本测试已失败，不能实现本次所需免重复密码 | 已排除为独立解决方案 |
| 启动时不立即读取、需要连接设备时再请求；正确区分授权失败与凭据不存在 | 减少后台或启动时打断，避免反复重试；不能自行获得机密或修复签名身份 | 补充体验改善 |
| 迁移 data protection keychain | Apple 推荐的新实现，但要处理授权的 access-group entitlements、provisioning 和旧数据迁移；不是加一项字段即可无缝解决当前身份问题 | 后续架构选项 |
| 固定凭据辅助程序，由它受控访问钥匙串 | 辅助程序内容保持不变可保持其 partition；必须验证调用程序签名、限定 IPC 和服务范围，且处理辅助程序更新 | 复杂架构备选，尚未实施验证 |
| 对凭据设置“允许所有应用访问” | 取消该机密的应用隔离；其他程序也可尝试读取 | 不建议 |
| ad hoc 签名配自定义“只检查 identifier”的 DR | 可人为放宽匹配条件，但相同 bundle identifier 可被其他程序仿用；它没有固定私钥的证明，可能让冒名代码满足机密的身份规则 | 不建议 |
| 改用普通配置文件或 `UserDefaults` 储存设备密钥 | 可避开钥匙串弹窗，却降低秘密存储保护 | 不建议作为正式解决方案 |

最初自签建议参考了 Apple 的归档 TN2206，其说明自签身份可用于 Keychain 的 DR 稳定性检查。但这不能作为现代 partition 机制的免密保证；本机实验和 Apple 的当前源码揭示了额外约束。自签具备有效稳定签名和实际钥匙串跨版本读取成功是不同的验收指标。[TN2206：macOS Code Signing In Depth](https://developer.apple.com/library/archive/technotes/tn2206/)、[Apple Security：clientid.cpp](https://github.com/apple-oss-distributions/Security/blob/main/securityd/src/clientid.cpp)

账号方面，Apple 支持通过免费 Apple Account 登录 Xcode，以 Personal Team 进行个人设备测试，但该流程存在 App ID、设备数和七天 provisioning 有效期等限制；Developer ID 与公证属于付费计划权益。当前应先确认用户 Xcode 账号与可用开发证书，不能把“必须付费”或“免费流程一定无配置限制”当作已确认事实。[Apple：Choosing a Membership](https://developer.apple.com/support/compare-memberships/)

Apple 官方同时说明 Apple Development 与 Developer ID 构建可能有不同 DR，因此应固定研发身份和签名类别，并检查实际 DR，而不是只保持证书显示名称。重复生成同名自签证书也不是复用同一证书。[TN3127：Inside Code Signing: Requirements](https://developer.apple.com/documentation/technotes/tn3127-inside-code-signing-requirements)

上述 identifier-only 方案的安全判断来自要求语义：identifier 条件只检查程序声明的签名标识，ad hoc 没有签名证书身份；若 ACL 仅保留这一条件，就缺少“来自同一私钥持有者”的约束。这是基于 Apple 要求语言的安全推断。[Code Signing Requirement Language](https://developer.apple.com/library/archive/documentation/Security/Conceptual/CodeSigningGuide/RequirementLang/RequirementLang.html)

ACL API 有容易踩的细节：`SecAccessCreate(..., nil, ...)` 默认只信任创建应用；`SecACLSetContents(..., nil, ...)` 的应用列表却表示任意应用可访问。前者不是禁用授权弹窗的开关。[SecAccessCreate](https://developer.apple.com/documentation/security/secaccesscreate(_:_:_:))、[SecACLSetContents](https://developer.apple.com/documentation/security/secaclsetcontents(_:_:_:_:))

提示抑制只能延后授权：`kSecUseAuthenticationUIFail` 文档规定需要交互时返回 `errSecInteractionNotAllowed`；不能因此获得未经授权的数据，而且不能直接假定这一参数能抑制当前 file-based `SecItem` shim 的弹窗。Chromium 当前源码记录这些 SecItem 抑制方式对 file-based keychain 不生效，采用保存/恢复 `SecKeychainSetUserInteractionAllowed` 状态的旧接口兜底（FB16959400）；这是使用方的实现经验，并非 Apple 对所有 macOS 版本的保证。若要实施，需在本机验证兼容性、作用范围和并发调用，且不能把交互失败当作“凭据丢失”后删除或重建凭据。[Apple：kSecUseAuthenticationUIFail](https://developer.apple.com/documentation/security/ksecuseauthenticationuifail)、[Chromium：scoped_keychain_user_interaction_allowed.cc](https://chromium.googlesource.com/chromium/src/crypto/+/refs/heads/main/apple/scoped_keychain_user_interaction_allowed.cc)

## 建议的实施和验收

1. 选择并固定用户已有或通过 Xcode 正规配置的 Apple Development / Developer ID Application 签名身份；验证证书真实 Apple 链、类别扩展和 team 标识，并校验 DR 不再绑定单次构建 `cdhash`。无可用证书时明确报错，避免无提示退回 ad hoc 或自签并声称已解决。
2. 按项目安装规则仍然用 `scripts/build-macos.sh` 替换 `/Applications/ThinCube.app`，不以改变启动路径规避问题。
3. 第一次由新身份读取旧凭据时，用户可能需输入一次登录钥匙串密码并选择“始终允许”。这是已有 ACL 的身份迁移，不应承诺绝对零次提示。
4. 先用一次性测试凭据做两次内容不同、同安装路径的构建；同时验证 DR 和跨版本钥匙串机密读取。通过后再验证完整应用的旧凭据保留、正常连接成功且不再弹当前提示。单独 `codesign` 校验通过不算解决本问题。
5. 另测拒绝授权的行为：保留凭据，明确显示授权状态，不循环弹窗、不自动删除条目。

使用真实证书签名后，`codesign` 访问签名私钥时也可能首次弹授权框；它的主体是 `codesign`、目标是签名私钥，与截图“ThinCube 读取 `app.thincube.device`”是两件事，应分别识别和处理。

## 本次实施结果

最终复用了 Xcode Personal Team 中已有的 Apple Development 证书，指纹 `82F78846A83B7129ECC93F167CAD9219C112C4CD`。初始沙盒内的 `security find-identity` 返回 0，不能据此认定本机没有可用身份；在正确访问环境中只读检查并通过 Xcode 界面确认了原有证书。最终没有为 Apple 身份创建或导入新私钥。

- `scripts/setup-macos-signing.sh` 选择已有 Apple 身份并保存公共证书、指纹；先用测试应用验证真实 Apple 证书链和类别，再更新固定指纹。再次执行不自动换证书。
- `scripts/sign-macos.sh` 固定 `identifier "app.thincube.mac" and certificate leaf = H"82f78846a83b7129ecc93f167cad9219c112c4cd"` 作为显式 DR，并独立验证 Apple 链、Mac Development / Developer ID 类别扩展和 TeamIdentifier。
- 构建与安装共用此校验；没有证书、证书无效、ad hoc 或自签均不能无提示覆盖系统应用。
- `scripts/test-macos-signing.sh` 编译两个不同内容的程序，确认 cdhash 不同、DR 相同且互相匹配；同路径替换后，新版在禁止用户交互的条件下成功读取旧版创建的一次性钥匙串测试项。拒绝 ad hoc 与缺失身份的检查通过，测试项删除成功。
- Swift 测试 54 项，0 失败、1 跳过（未指定真实固件镜像）。最终 release 编译、固定 Apple 签名和 `/Applications/ThinCube.app` 替换、重启通过，实际 TeamIdentifier 为 `U2WPKR9Q68`。
- 原生应用界面已确认原设备 `ThinCube-6046E5` 恢复 BLE 在线，显示“已连接，语音待命”；此次未在应用中输入配对码或触发重新配对。
- 再次运行正式 `scripts/install-macos.sh` 替换同一应用包并重启后，原设备再次自动恢复 BLE 在线。该真实应用检查验证安装/重启与原配对保留；不同代码内容的更新访问验证由前述两个临时程序完成，不混淆两类实验。
- 已删除本次创建且弃用的自签测试证书、对应新私钥及用户代码签名信任设置；最终仅保留原有 Apple Development 签名身份。

真实设备配对项保持原存储方式，调查工具未直接读取或修改其机密内容，正式应用仍通过原 Credentials 逻辑恢复连接。首次由原 ad hoc 身份切换到 Apple 身份时，系统可能仍需一次“始终允许”授权；此次未记录用户对首次系统弹窗的实际操作，不能声称初次身份迁移完全没有提示。跨版本无交互访问由临时凭据测试验证，真实应用已确认原配对设备恢复在线。
