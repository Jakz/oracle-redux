# Preserve indexed texels through palette resolution

Status: Accepted

The production renderer keeps decoded Game Boy tile and sprite texels as
palette indices in single-channel GPU atlases. Instances select the applicable
background or sprite palette and retain flip, bank, priority, and transparency
semantics. Palette resolution occurs in the world shader before lighting and
post-processing; palette changes therefore update palette data rather than
regenerating RGBA atlases. Base texels use exact nearest sampling. Sprite color
index zero is transparent, while background color index zero remains an opaque
palette entry unless a separately documented effect specifies otherwise.
Precolored RGBA rooms and sprite sheets remain permitted as diagnostic outputs
or derived caches, but are not canonical production assets.
