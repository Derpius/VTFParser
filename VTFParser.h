#pragma once

#include "FileFormat/Structs.h"

#include <cstdint>

class VTFTexture
{
private:
	VTFHeader* mpHeader;
	uint8_t* mpImageData;
	uint32_t mImageDataSize = 0;

	bool mIsValid = false;

public:
	/// <summary>
	/// VTFTexture class
	/// </summary>
	/// <param name="pData">Pointer to char buffer that represents a VTF image</param>
	/// <param name="size">Size of the buffer</param>
	/// <param name="headerOnly">Whether to just parse the header or not (default: false)</param>
	VTFTexture(const uint8_t* pData, size_t size, bool headerOnly = false);
	~VTFTexture();

	/// <summary>
	/// Returns whether or not the image is valid
	/// </summary>
	/// <returns>True if the header and image data were read successfully</returns>
	bool IsValid() const;

	ImageFormatInfo GetFormat() const;
	uint32_t GetVersionMajor() const;
	uint32_t GetVersionMinor() const;

	/// <summary>
	/// Gets the width of the image at the specified mip level
	/// </summary>
	/// <param name="mipLevel">Mipmap to get the width of (default: 0)</param>
	/// <returns>Width of the image in pixels</returns>
	uint16_t GetWidth(uint8_t mipLevel = 0) const;

	/// <summary>
	/// Gets the height of the image at the specified mip level
	/// </summary>
	/// <param name="mipLevel">Mipmap to get the height of (default: 0)</param>
	/// <returns>Height of the image in pixels</returns>
	uint16_t GetHeight(uint8_t mipLevel = 0) const;

	/// <summary>
	/// Gets the depth of the image at the specified mip level
	/// </summary>
	/// <param name="mipLevel">Mipmap to get the depth of (default: 0)</param>
	/// <returns>Depth of the image in pixels</returns>
	uint16_t GetDepth(uint8_t mipLevel = 0) const;

	uint16_t GetMIPLevels() const;

	uint16_t GetFrames() const;
	uint16_t GetFirstFrame() const;

	/// <summary>
	/// Gets a pixel from the image at the specified coordinate, MIP level, frame, and face
	/// </summary>
	/// <param name="x">Coordinate of the pixel on the x axis</param>
	/// <param name="y">Coordinate of the pixel on the y axis</param>
	/// <param name="z">Coordinate of the pixel on the z axis (volumetric textures only)</param>
	/// <param name="mipLevel">MIP level to read (automatically transforms above coordinates)</param>
	/// <param name="frame">Frame of the image (animated textures only)</param>
	/// <param name="face">Face of the image (envmaps only)</param>
	/// <returns>VTFPixel struct with the pixel data</returns>
	VTFPixel GetPixel(uint16_t x, uint16_t y, uint16_t z, uint8_t mipLevel, uint16_t frame, uint8_t face) const;

	/// <summary>
	/// Gets a pixel from the image at the specified coordinate, MIP level, and frame of an animated 2D texture
	/// </summary>
	/// <param name="x">Coordinate of the pixel on the x axis</param>
	/// <param name="y">Coordinate of the pixel on the y axis</param>
	/// <param name="mipLevel">MIP level to read (automatically transforms above coordinates)</param>
	/// <param name="frame">Frame of the image</param>
	/// <returns>VTFPixel struct with the pixel data</returns>
	VTFPixel GetPixel(uint16_t x, uint16_t y, uint8_t mipLevel, uint16_t frame) const;

	/// <summary>
	/// Gets a pixel from the image at the specified coordinate and MIP level of a standard 2D texture
	/// </summary>
	/// <param name="x">Coordinate of the pixel on the x axis</param>
	/// <param name="y">Coordinate of the pixel on the y axis</param>
	/// <param name="mipLevel">MIP level to read (automatically transforms above coordinates)</param>
	/// <returns>VTFPixel struct with the pixel data</returns>
	VTFPixel GetPixel(uint16_t x, uint16_t y, uint8_t mipLevel) const;
};
