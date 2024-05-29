﻿#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "winrt.hpp"
#include "wucUtils.hpp"
#include "ColorSourceEffect.hpp"
#include "OpacityEffect.hpp"
#include "BlendEffect.hpp"
#include "TintEffect.hpp"
#include "SaturationEffect.hpp"
#include "GaussianBlurEffect.hpp"
#include "CompositeEffect.hpp"

namespace OpenGlass::AeroBackdrop
{
	wuc::CompositionBrush CreateBrush(
		const wuc::Compositor& compositor,
		const wu::Color& color,
		const wu::Color& afterglowColor,
		float colorBalance,
		float afterglowBalance,
		float blurBalance,
		float blurAmount
	)
	{
		if (static_cast<float>(color.A) * colorBalance == 255.f)
		{
			return compositor.CreateColorBrush(color);
		}

		wuc::CompositionBrush blurredBackdropBrush
		{
			[](auto compositor, auto blurAmount) -> wuc::CompositionBrush
			{
				if (!blurAmount)
				{
					return compositor.CreateBackdropBrush();
				}

				auto gaussianBlurEffect{ winrt::make_self<Win2D::GaussianBlurEffect>() };
				gaussianBlurEffect->SetName(L"Blur");
				gaussianBlurEffect->SetBorderMode(D2D1_BORDER_MODE_HARD);
				gaussianBlurEffect->SetBlurAmount(blurAmount);
				gaussianBlurEffect->SetOptimizationMode(D2D1_GAUSSIANBLUR_OPTIMIZATION_SPEED);
				gaussianBlurEffect->SetInput(wuc::CompositionEffectSourceParameter{ L"Backdrop" });

				auto blurredBackdropBrush{ compositor.CreateEffectFactory(*gaussianBlurEffect).CreateBrush() };
				blurredBackdropBrush.SetSourceParameter(L"Backdrop", compositor.CreateBackdropBrush());

				return blurredBackdropBrush;
			} (compositor, blurAmount)
		};

		// the current recipe is modified from @kfh83, @TorutheRedFox, @aubymori
		auto fallbackTintSource{ winrt::make_self<Win2D::ColorSourceEffect>() };
		fallbackTintSource->SetColor(wu::Color
		{
			255,
			static_cast<UCHAR>(min(blurBalance + 0.1f, 1.f) * 255.f),
			static_cast<UCHAR>(min(blurBalance + 0.1f, 1.f) * 255.f),
			static_cast<UCHAR>(min(blurBalance + 0.1f, 1.f) * 255.f),
		});

		auto blackOrTransparentSource{ winrt::make_self<Win2D::TintEffect>() };
		blackOrTransparentSource->SetInput(wuc::CompositionEffectSourceParameter{ L"BlurredBackdrop" });
		blackOrTransparentSource->SetColor(D2D1::ColorF(D2D1::ColorF::Black));

		auto colorEffect{ winrt::make_self<Win2D::ColorSourceEffect>() };
		colorEffect->SetName(L"MainColor");
		colorEffect->SetColor(color);

		auto colorOpacityEffect{ winrt::make_self<Win2D::OpacityEffect>() };
		colorOpacityEffect->SetName(L"MainColorOpacity");
		colorOpacityEffect->SetInput(*colorEffect);
		colorOpacityEffect->SetOpacity(colorBalance);

		auto blurredBackdropBalanceEffect{ winrt::make_self<Win2D::OpacityEffect>() };
		blurredBackdropBalanceEffect->SetName(L"BlurBalance");
		blurredBackdropBalanceEffect->SetOpacity(blurBalance);
		blurredBackdropBalanceEffect->SetInput(wuc::CompositionEffectSourceParameter{ L"BlurredBackdrop" });

		auto actualBackdropEffect{ winrt::make_self<Win2D::CompositeStepEffect>() };
		actualBackdropEffect->SetCompositeMode(D2D1_COMPOSITE_MODE_PLUS);
		actualBackdropEffect->SetDestination(*blackOrTransparentSource);
		actualBackdropEffect->SetSource(*blurredBackdropBalanceEffect);

		auto desaturatedBlurredBackdrop{ winrt::make_self<Win2D::SaturationEffect>() };
		desaturatedBlurredBackdrop->SetSaturation(0.f);
		desaturatedBlurredBackdrop->SetInput(wuc::CompositionEffectSourceParameter{ L"BlurredBackdrop" });

		// make animation feel better...
		auto backdropNotTransparentPromised{ winrt::make_self<Win2D::CompositeStepEffect>() };
		backdropNotTransparentPromised->SetCompositeMode(D2D1_COMPOSITE_MODE_SOURCE_OVER);
		backdropNotTransparentPromised->SetDestination(*fallbackTintSource);
		backdropNotTransparentPromised->SetSource(*desaturatedBlurredBackdrop);

		// if the afterglowColor is black, then it will produce a completely transparent surface
		auto tintEffect{ winrt::make_self<Win2D::TintEffect>() };
		tintEffect->SetInput(*backdropNotTransparentPromised);
		tintEffect->SetColor(wu::Color{ static_cast<UCHAR>(static_cast<float>(afterglowColor.A) * afterglowBalance), afterglowColor.R, afterglowColor.G, afterglowColor.B });

		auto backdropWithAfterGlow{ winrt::make_self<Win2D::CompositeStepEffect>() };
		backdropWithAfterGlow->SetCompositeMode(D2D1_COMPOSITE_MODE_PLUS);
		backdropWithAfterGlow->SetDestination(*actualBackdropEffect);
		backdropWithAfterGlow->SetSource(*tintEffect);

		auto compositeEffect{ winrt::make_self<Win2D::CompositeStepEffect>() };
		compositeEffect->SetCompositeMode(D2D1_COMPOSITE_MODE_PLUS);
		compositeEffect->SetDestination(*backdropWithAfterGlow);
		compositeEffect->SetSource(*colorOpacityEffect);

		auto effectBrush{ compositor.CreateEffectFactory(*compositeEffect).CreateBrush() };
		effectBrush.SetSourceParameter(L"BlurredBackdrop", blurredBackdropBrush);

		return effectBrush;
	}
}