# MiaoZip v1.0 图标

以用户提供的“拉链止于鼻子”猫脸方案为设计来源，使用内置 imagegen 编辑为独立图标。保留透明圆角外缘，去掉设计稿说明文字，蓝色底板使用单色处理。随后仅缩放、居中并封装为 Windows PNG / ICO 资源。

- 主图：`Assets/MiaoZip.png`
- ICO：`Assets/MiaoZip.ico`
- ICO 尺寸：16、20、24、32、40、48、64、96、128、256 像素。
- 安装包图像：370 个原尺寸资源，保留透明度。
- 应用内部使用自己的资源，不修改系统共享图像列表或 Windows 文件关联。

Windows 安装包要求每张清单图像不超过 200 KiB。32 张较大的派生 PNG 使用 256 色索引编码以满足该限制，其余 338 张保留 RGBA；尺寸和透明外缘保持一致。1254 像素主图与十尺寸 ICO 未经过该转换。编码比较和资源校验见发布验证记录。

主图 SHA256：94122C0357FCA422F004CAF5E85DD12970A481D58A405687E46A3992A83D3A37

ICO SHA256：EC999A747E886DD2C668C2D319198D7956FDEAFDC225DCC1AA168552A8460AAC

## 最终编辑提示词

Use case: precise-object-edit. Edit target: the supplied MiaoZip icon design sheet. Produce ONE finished standalone square Windows application icon, based exactly on the large cat-face icon in this sheet. Remove the presentation sheet, all writing, and the small size examples. Preserve the recognizable white cat face, pointed ears with cyan ear interiors, two dark blue round eyes, gold zipper descending down the center of the forehead, and gold zipper pull forming the cat nose, stopping at the nose; preserve the blue curved mouth. Change the rounded-square tile behind the cat into a perfectly uniform solid cyan-blue color #099ED0, with no blue-to-cyan gradient, no light rim, no bevel, no glow, and no cast shadow. The white face should be clean solid white, the nose/zipper warm gold, outlines and eyes dark blue. A flat, crisp, polished app icon faithful to the supplied silhouette. Center the single icon and let the rounded tile occupy 94 percent of the square canvas, with actual transparency outside its rounded corners and small even transparent margins. No text, no watermark, no other objects. Keep the cat design; change only the presentation framing and the color treatment needed for this solid-color application asset.
