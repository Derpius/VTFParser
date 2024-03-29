﻿#include "VTFParser.h"
#include "FileFormat/Parser.h"
#include "DXTn/DXTn.h"

#include <stdexcept>
#include <cmath>
#include <algorithm>

inline int intmod(int a, int b) {
	return (a % b + b) % b;
}

VTFTexture::VTFTexture(const uint8_t* pData, size_t size, bool headerOnly)
{
	mpImageData = nullptr;
	mpHeader = new VTFHeader;

	mIsValid = VTFParser::ParseHeader(pData, size, mpHeader);
	if (!mIsValid || headerOnly) return;

	uint8_t* pCompressedImageData;
	mIsValid = VTFParser::ParseImageData(pData, size, mpHeader, &pCompressedImageData, &mImageDataSize);
	if (!mIsValid) return;

	if (VTFParser::GetImageFormatInfo(mpHeader->highResImageFormat).isCompressed) {
		mImageDataSize = VTFParser::CalcImageSize(
			mpHeader->width, mpHeader->height,
			mpHeader->depth, mpHeader->mipmapCount,
			IMAGE_FORMAT::RGBA8888
		) * mpHeader->frames * VTFParser::GetFaceCount(mpHeader);
		mpImageData = reinterpret_cast<uint8_t*>(malloc(mImageDataSize));
		if (mpImageData == nullptr) {
			mIsValid = false;
			free(pCompressedImageData);
			return;
		}

		// for each mipmap (small to large)
		// for each frame
		// for each face
		// for each z slice
		// decompress image into final data using switch
		uint32_t compOffset = 0, uncompOffset = 0;
		for (int16_t mipmap = mpHeader->mipmapCount - 1; mipmap >= 0; mipmap--) {
			uint16_t depth = mpHeader->depth >> mipmap;
			if (depth < 1)  depth = 1;

			for (uint16_t frame = 0; frame < mpHeader->frames; frame++) {
				for (uint8_t face = 0; face < VTFParser::GetFaceCount(mpHeader); face++) {
					for (uint16_t slice = 0; slice < depth; slice++) {
						uint16_t width = mpHeader->width >> mipmap, height = mpHeader->height >> mipmap;

						if (width < 1)  width = 1;
						if (height < 1) height = 1;

						switch (mpHeader->highResImageFormat) {
						case IMAGE_FORMAT::DXT1:
						case IMAGE_FORMAT::DXT1_ONEBITALPHA:
							DXTn::DecompressDXT1(pCompressedImageData + compOffset, mpImageData + uncompOffset, width, height);
							break;
						case IMAGE_FORMAT::DXT3:
							DXTn::DecompressDXT3(pCompressedImageData + compOffset, mpImageData + uncompOffset, width, height);
							break;
						case IMAGE_FORMAT::DXT5:
							DXTn::DecompressDXT5(pCompressedImageData + compOffset, mpImageData + uncompOffset, width, height);
							break;
						default:
							mIsValid = false;
							free(pCompressedImageData);
							return;
						}

						compOffset += VTFParser::CalcImageSize(width, height, 1, mpHeader->highResImageFormat);
						uncompOffset += VTFParser::CalcImageSize(width, height, 1, IMAGE_FORMAT::RGBA8888);
					}
				}
			}
		}

		mpHeader->highResImageFormat = IMAGE_FORMAT::RGBA8888;
		free(pCompressedImageData);
	} else {
		mpImageData = reinterpret_cast<uint8_t*>(malloc(mImageDataSize));
		if (mpImageData == nullptr) {
			mIsValid = false;
			free(pCompressedImageData);
			return;
		} else {
			memcpy(mpImageData, pCompressedImageData, mImageDataSize);
			free(pCompressedImageData);
		}
	}
}

VTFTexture::VTFTexture(const VTFTexture& src)
{
	mpHeader = new VTFHeader;
	memcpy(mpHeader, src.mpHeader, sizeof(VTFHeader));

	if (src.mIsValid) {
		mImageDataSize = src.mImageDataSize;
		mpImageData = static_cast<uint8_t*>(malloc(mImageDataSize));
		memcpy(mpImageData, src.mpImageData, mImageDataSize);
		mIsValid = true;
	}
}

VTFTexture::~VTFTexture()
{
	delete mpHeader;
	if (mpImageData != nullptr) free(mpImageData);
}

bool VTFTexture::IsValid() const { return mIsValid; }

ImageFormatInfo VTFTexture::GetFormat() const
{
	return IsValid() ? VTFParser::GetImageFormatInfo(mpHeader->highResImageFormat) : VTFParser::GetImageFormatInfo(IMAGE_FORMAT::NONE);
}
uint32_t VTFTexture::GetVersionMajor() const
{
	return IsValid() ? mpHeader->version[0] : 0;
}
uint32_t VTFTexture::GetVersionMinor() const
{
	return IsValid() ? mpHeader->version[1] : 0;
}

uint16_t VTFTexture::GetWidth(uint8_t mipLevel) const
{
	uint16_t width = mpHeader->width >> mipLevel;
	if (width < 1) width = 1;
	return IsValid() ? width : 0;
}
uint16_t VTFTexture::GetHeight(uint8_t mipLevel) const
{
	uint16_t height = mpHeader->height >> mipLevel;
	if (height < 1) height = 1;
	return IsValid() ? height : 0;
}
uint16_t VTFTexture::GetDepth(uint8_t mipLevel) const
{
	uint16_t depth = mpHeader->depth >> mipLevel;
	if (depth < 1) depth = 1;
	return IsValid() ? depth : 0;
}

uint8_t VTFTexture::GetFaces() const
{
	return IsValid() ? VTFParser::GetFaceCount(mpHeader) : 0;
}

uint16_t VTFTexture::GetMIPLevels() const
{
	return IsValid() ? mpHeader->mipmapCount : 0;
}

uint16_t VTFTexture::GetFrames() const
{
	return IsValid() ? mpHeader->frames : 0;
}
uint16_t VTFTexture::GetFirstFrame() const
{
	return IsValid() ? mpHeader->firstFrame : 0;
}

VTFPixel VTFTexture::GetPixel(uint16_t x, uint16_t y, uint16_t z, uint8_t mipLevel, uint16_t frame, uint8_t face) const
{
	if (!IsValid()) return VTFPixel{};

	// Image data offset
	uint32_t offset = 0;

	uint16_t width = mpHeader->width >> mipLevel;
	uint16_t height = mpHeader->height >> mipLevel;
	uint16_t depth = mpHeader->depth >> mipLevel;

	for (uint8_t i = mipLevel + 1; i < mpHeader->mipmapCount; i++) {
		width >>= 1;
		height >>= 1;
		depth >>= 1;

		if (width < 1)  width = 1;
		if (height < 1) height = 1;
		if (depth < 1)  depth = 1;

		offset += VTFParser::CalcImageSize(width, height, depth, mpHeader->highResImageFormat) * VTFParser::GetFaceCount(mpHeader) * mpHeader->frames;
	}

	width = mpHeader->width >> mipLevel;
	height = mpHeader->height >> mipLevel;
	depth = mpHeader->depth >> mipLevel;

	if (width < 1)  width = 1;
	if (height < 1) height = 1;
	if (depth < 1)  depth = 1;

	uint32_t pixelSize = VTFParser::GetImageFormatInfo(mpHeader->highResImageFormat).bytesPerPixel;
	uint32_t sliceSize = width * height * pixelSize;
	uint32_t faceSize = sliceSize * depth;
	uint32_t frameSize = faceSize * VTFParser::GetFaceCount(mpHeader);
	offset += frame * frameSize + face * faceSize + z * sliceSize + y * width * pixelSize + x * pixelSize;

	return VTFParser::ParsePixel(mpImageData + offset, mpHeader->highResImageFormat);
}

VTFPixel VTFTexture::SampleBilinear(float u, float v, uint16_t z, uint8_t mipLevel, uint16_t frame, uint8_t face) const
{
	if (!IsValid()) return VTFPixel{};

	// Image data offset
	uint32_t offset = 0;

	uint16_t width = mpHeader->width >> mipLevel;
	uint16_t height = mpHeader->height >> mipLevel;
	uint16_t depth = mpHeader->depth >> mipLevel;

	// TODO: Cache these mip offsets on image load and perform a lookup here
	for (uint8_t i = mipLevel + 1; i < mpHeader->mipmapCount; i++) {
		width >>= 1;
		height >>= 1;
		depth >>= 1;

		if (width < 1)  width = 1;
		if (height < 1) height = 1;
		if (depth < 1)  depth = 1;

		offset += VTFParser::CalcImageSize(width, height, depth, mpHeader->highResImageFormat) * VTFParser::GetFaceCount(mpHeader) * mpHeader->frames;
	}

	width = mpHeader->width >> mipLevel;
	height = mpHeader->height >> mipLevel;
	depth = mpHeader->depth >> mipLevel;

	if (width < 1)  width = 1;
	if (height < 1) height = 1;
	if (depth < 1)  depth = 1;

	uint32_t pixelSize = VTFParser::GetImageFormatInfo(mpHeader->highResImageFormat).bytesPerPixel;
	uint32_t sliceSize = width * height * pixelSize;
	uint32_t faceSize = sliceSize * depth;
	uint32_t frameSize = faceSize * VTFParser::GetFaceCount(mpHeader);
	offset += frame * frameSize + face * faceSize + z * sliceSize;// +y * width * pixelSize + x * pixelSize;

	VTFPixel filtered{};

	bool clampX = (mpHeader->flags & static_cast<uint32_t>(TEXTURE_FLAGS::CLAMPS)) != 0;
	bool clampY = (mpHeader->flags & static_cast<uint32_t>(TEXTURE_FLAGS::CLAMPT)) != 0;

	// Remap to 0-1
	if (clampX)
		u = std::clamp(u, 0.f, 0.9999f);
	else
		u -= floorf(u);

	if (clampY)
		v = std::clamp(v, 0.f, 0.9999f);
	else
		v -= floorf(v);

	// Remap to pixel centres
	u = u * width - 0.5f;
	v = v * height - 0.5f;

	// Floor to nearest pixel
	int x = floorf(u);
	int y = floorf(v);

	// Calculate fractional coordinate and inverse
	float uFract = u - x;
	float vFract = v - y;
	float uFractInv = 1.f - uFract;
	float vFractInv = 1.f - vFract;

	VTFPixel corners[2][2];
	for (int xOff = 0; xOff < 2; xOff++) {
		for (int yOff = 0; yOff < 2; yOff++) {
			int xCorner = x + xOff, yCorner = y + yOff;
			if (clampX)
				xCorner = std::clamp(xCorner, 0, static_cast<int>(width) - 1);
			else
				xCorner = intmod(xCorner, width);

			if (clampY)
				yCorner = std::clamp(yCorner, 0, static_cast<int>(height) - 1);
			else
				yCorner = intmod(yCorner, height);

			corners[xOff][yOff] = VTFParser::ParsePixel(
				mpImageData + offset + yCorner * width * pixelSize + xCorner * pixelSize,
				mpHeader->highResImageFormat
			);
		}
	}

	return VTFPixel{
		(corners[0][0].r * uFractInv + corners[1][0].r * uFract) * vFractInv +
		(corners[0][1].r * uFractInv + corners[1][1].r * uFract) * vFract,

		(corners[0][0].g * uFractInv + corners[1][0].g * uFract)* vFractInv +
		(corners[0][1].g * uFractInv + corners[1][1].g * uFract) * vFract,

		(corners[0][0].b * uFractInv + corners[1][0].b * uFract)* vFractInv +
		(corners[0][1].b * uFractInv + corners[1][1].b * uFract) * vFract,

		(corners[0][0].a * uFractInv + corners[1][0].a * uFract)* vFractInv +
		(corners[0][1].a * uFractInv + corners[1][1].a * uFract) * vFract,
	};
}

VTFPixel VTFTexture::Sample(float u, float v, uint16_t z, float mipLevel, uint16_t frame, uint8_t face) const
{
	mipLevel = std::clamp(mipLevel, 0.f, static_cast<float>(mpHeader->mipmapCount - 1));
	float mipHigh = floorf(mipLevel), mipLow = ceilf(mipLevel);

	VTFPixel high = SampleBilinear(u, v, z, mipHigh, frame, face);
	if (mipLow == mipHigh) return high;

	VTFPixel low = SampleBilinear(u, v, z, mipLow, frame, face);

	float fract = mipLevel - mipHigh;
	float fractInv = 1.f - fract;

	return VTFPixel{
		low.r * fract + high.r * fractInv,
		low.g * fract + high.g * fractInv,
		low.b * fract + high.b * fractInv,
		low.a * fract + high.a * fractInv
	};
}
