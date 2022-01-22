#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"

#define OLC_PGEX_TRANSFORMEDVIEW
#include "olcPGEX_TransformedView.h"

struct Button
{
	olc::vi2d pos;
	olc::vi2d size;

	std::string text;

	bool clicked(olc::vi2d mousePos)
	{
		return mousePos.x >= pos.x
			&& mousePos.x <= (pos.x + size.x)
			&& mousePos.y >= pos.y
			&& mousePos.y <= (pos.y + size.y);
	}

	void draw(olc::PixelGameEngine* pge)
	{
		pge->FillRect(pos, size, olc::GREEN);
		pge->DrawRect(pos, size, olc::RED);

		olc::vi2d textSize = pge->GetTextSizeProp(text);
		pge->DrawStringProp(pos + (size - textSize)/4, text, olc::RED, 2U);
	}
};

class Floyd_Steinberg_Dithering : public olc::PixelGameEngine
{
public:
	Floyd_Steinberg_Dithering()
	{
		sAppName = "Floyd_Steinberg_Dithering";
	}

	olc::TransformedView tv;
	std::unique_ptr<olc::Sprite> m_pImageOriginal;
	std::unique_ptr<olc::Sprite> m_pImageGreyscaled;
	std::unique_ptr<olc::Sprite> m_pImageQuantized;
	std::unique_ptr<olc::Sprite> m_pImageDithered;

	Button ButtonGreyscale;
	Button ButtonQuantized;
	Button ButtonDithered;

	olc::vi2d buttonSize = {170, 40};

	int mode = 0;


public:
	bool OnUserCreate() override
	{

		tv.Initialise({ScreenWidth(), ScreenHeight()});
		m_pImageOriginal = std::make_unique<olc::Sprite>("./images/pic2.png");
		m_pImageGreyscaled = std::make_unique<olc::Sprite>(m_pImageOriginal->width, m_pImageOriginal->height);
		m_pImageQuantized = std::make_unique<olc::Sprite>(m_pImageOriginal->width, m_pImageOriginal->height);
		m_pImageDithered = std::make_unique<olc::Sprite>(m_pImageOriginal->width, m_pImageOriginal->height);
	
		ButtonGreyscale.pos = {20, 20};
		ButtonGreyscale.size = buttonSize;
		ButtonGreyscale.text = "Greyscale";

		ButtonQuantized.pos = {40 + buttonSize.x, 20};
		ButtonQuantized.size = buttonSize;
		ButtonQuantized.text = "Quantize";

		ButtonDithered.pos = {60 + 2 * buttonSize.x, 20};
		ButtonDithered.size = buttonSize;
		ButtonDithered.text = "Dither";



		auto Convert_RGB_To_GreyScale = [](const olc::Pixel in)
		{
			uint8_t greyscale = uint8_t(0.2162f * float(in.r) 
									+ 0.7152f * float(in.g) 
									+ 0.0722f * float(in.b));

			return olc::Pixel(greyscale, greyscale, greyscale);
		};

		std::transform(
			m_pImageOriginal->pColData.begin(), 
			m_pImageOriginal->pColData.end(), 
			m_pImageGreyscaled->pColData.begin(), 
			Convert_RGB_To_GreyScale);
		

		//quantize to 1 bit per pixel
		auto Quantize_Greyscale_1Bit = [](const olc::Pixel in)
		{
			return in.r < 125 ? olc::BLACK : olc::WHITE;
		};

		auto Quantize_Greyscale_nBit = [](const olc::Pixel in)
		{
			constexpr int nBits = 4;
			constexpr float fLevels = (1 << nBits) - 1;

			uint8_t c = uint8_t(
				std::clamp(
					std::round(float(in.r) / 255.0f * fLevels) / fLevels * 255.0f, 
					0.0f, 255.0f
				)
			);

			return olc::Pixel(c, c, c);
		};

		auto Quantize_RGB_nBit = [](const olc::Pixel in)
		{
			constexpr int nBits = 2;
			constexpr float fLevels = (1 << nBits) - 1;

			uint8_t cr = uint8_t(
				std::clamp(
					std::round(float(in.r) / 255.0f * fLevels) / fLevels * 255.0f, 
					0.0f, 255.0f
				)
			);

			uint8_t cg = uint8_t(
				std::clamp(
					std::round(float(in.g) / 255.0f * fLevels) / fLevels * 255.0f, 
					0.0f, 255.0f
				)
			);

			uint8_t cb = uint8_t(
				std::clamp(
					std::round(float(in.b) / 255.0f * fLevels) / fLevels * 255.0f, 
					0.0f, 255.0f
				)
			);

			return olc::Pixel(cr, cg, cb);
		};

		auto Quantise_RGB_CustomPalette = [](const olc::Pixel in)
		{
			std::array<olc::Pixel, 8> nShades = { olc::BLACK, olc::WHITE, olc::RED, olc::GREEN, olc::BLUE, olc::YELLOW, olc::MAGENTA, olc::CYAN };
			
			float fClosest = INFINITY;
			olc::Pixel pClosest;

			for (const auto& c : nShades)
			{
				float fDistance = float(
					std::sqrt(
						std::pow(float(c.r) - float(in.r), 2) +
						std::pow(float(c.g) - float(in.g), 2) +
						std::pow(float(c.b) - float(in.b), 2)));

				if (fDistance < fClosest)
				{
					fClosest = fDistance;
					pClosest = c;
				}
			}
						
			return pClosest;
		};
		
		auto quantizingFunction = Quantise_RGB_CustomPalette;

		std::transform(
			m_pImageOriginal->pColData.begin(), 
			m_pImageOriginal->pColData.end(), 
			m_pImageQuantized->pColData.begin(), 
			quantizingFunction);

		FloydSteinbergDithering(quantizingFunction, m_pImageOriginal.get(), m_pImageDithered.get());

		return true;
	}

	void FloydSteinbergDithering(
		std::function<olc::Pixel(const olc::Pixel)> QuantizingFunction,
		const olc::Sprite* pSource, olc::Sprite* pDest)
	{
		std::copy(pSource->pColData.begin(), pSource->pColData.end(), pDest->pColData.begin());

		olc::vi2d vPixel;
	
		for (vPixel.y = 0; vPixel.y < pSource->height; vPixel.y++)
		{
			for (vPixel.x = 0; vPixel.x < pSource->width; vPixel.x++)
			{
				olc::Pixel op = pDest->GetPixel(vPixel);
				olc::Pixel qp = QuantizingFunction(op);

				int32_t error[3] = 
				{
					op.r - qp.r,
					op.g - qp.g,
					op.b - qp.b
				};

				pDest->SetPixel(vPixel, qp);

				auto SetDitheredPixel = [&vPixel, &pDest, &error](const olc::vi2d offset, const float bias)
				{
					olc::Pixel p = pDest->GetPixel(vPixel + offset);
					int32_t k[3] = {p.r, p.g, p.b};
					k[0] += int32_t(bias * float(error[0]));
					k[1] += int32_t(bias * float(error[1]));
					k[2] += int32_t(bias * float(error[2]));
					pDest->SetPixel(vPixel + offset, olc::Pixel(std::clamp(k[0], 0, 255), std::clamp(k[1], 0, 255), std::clamp(k[2], 0, 255)));

				};

				SetDitheredPixel({1, 0}, 7.0f/16.0f);
				SetDitheredPixel({1, 1}, 1.0f/16.0f);
				SetDitheredPixel({0, 1}, 5.0f/16.0f);
				SetDitheredPixel({-1, 1}, 3.0f/16.0f);

			}
		}
	}

	bool OnUserUpdate(float fElapsedTime) override
	{
		tv.HandlePanAndZoom();

		Clear(olc::BLACK);


		if (GetMouse(0).bPressed)
		{
			if (ButtonGreyscale.clicked(GetMousePos()))
			{
				mode = 1;
			} else if (ButtonQuantized.clicked(GetMousePos()))
			{
				mode = 2;
			
			} else if (ButtonDithered.clicked(GetMousePos()))
			{
				mode = 3;
			} else
			{
				mode = 0;
			}
		}

		switch (mode)
		{
		case 0:
			tv.DrawSprite({0, 0}, m_pImageOriginal.get());
			break;
		case 1:
			tv.DrawSprite({0, 0}, m_pImageGreyscaled.get());
			break;
		case 2:
			tv.DrawSprite({0, 0}, m_pImageQuantized.get());
			break;
		case 3:
			tv.DrawSprite({0, 0}, m_pImageDithered.get());
			break;
		
		default:
			break;
		}

		ButtonGreyscale.draw(this);
		ButtonQuantized.draw(this);
		ButtonDithered.draw(this);
		return true;
	}
};


int main()
{
	Floyd_Steinberg_Dithering demo;
	if (demo.Construct(1920, 1080, 1, 1))
		demo.Start();

	return 0;
}
