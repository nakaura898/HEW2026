#include "ui_button.h"
#include "engine/input/input_manager.h"
#include "engine/input/key.h"
#include "engine/debug/debug_draw.h"


UIButton::UIButton(const Vector2& pos, const Vector2& size)
	:position_(pos)
	,size_(size)
{
}

//マウスがボタンの範囲内か判定
bool UIButton::IsMouseOver() const
{
	InputManager* input = InputManager::GetInstance();
	if (!input) return false;

	//マウスの座標取得
	Vector2 mouse = input->GetMouse().GestPosition();

	//ボタンの範囲を計算
	float left = position_.x - size_.x * 0.5f;
	float right = position_.x + size_.x * 0.5f;
	float top = position_.y - size_.y * 0.5f;
	float bottom = position_.y + size_.y * 0.5f;

	//マウスが範囲内か？
	bool inX = (mouse.x >= left) && (mouse.x <= right);
	bool inY = (mouse.y >= top) && (mouse.y <= bottom);

	return inX && inY;
}

void UIButton::Update()
{
	InputManager* input = InputManager::GetInstance();
	if (!input)return;

	//マウスがボタンの範囲内ならクリック判定
	if (IsMouseOver())
	{
		//マウスの左クリックが押されたら
		if (input->GetMouse().IsButtonDown(MouseButton::Left))
		{
			if (onClick_)
			{
				onClick_();
			}
		}
	}
}

void UIButton::Render()
{
	InputManager* input = InputManager::GetInstance();

	//マウスの状態に応じて、ボタンの色を変更
	
	//通常
	Color color = normalColor_;
	
	//マウスがボタンの範囲内ならクリック判定
	if (IsMouseOver())
	{
		//押下しているか
		if (input->GetMouse().IsButtonPressed(MouseButton::Left))
		{
			//押下
			color = pressColor_;
		}
		else
		{
			//ホバー
			color = hoverColor_;
		}
	}

	currentColor_ = color;//テスト
}