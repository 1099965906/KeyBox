#include "stdafx.h"
#include <locale>
#include <codecvt>
#include <string>
#include <chrono>   
#include <thread>
#include <functional>
#include <mutex>
#include <sstream>
#include <memory>
#include "KeyParser.h"
#include "operate/Mouse.h"
#include "event/KeyEvent.h"
#include "manager/KeyManager.h"
#include "event/SleepEvent.h"
#include "event/StopEvent.h"
#include "event/MouseEvent.h"
using namespace std::chrono;

extern std::mutex mutex;

static const std::wstring GetWstr(const char *c) {
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	std::wstring wc = converter.from_bytes(std::string(c));
	return wc;
}

// ��ͨ¼����
bool onRecord(int isKey, const Mouse & mouse, const Key & key, KeyEventState state) {
	////////////////////////////////////////////////////////////////////////////////
	// ¼��ǰ׼������
	////////////////////////////////////////////////////////////////////////////////

	// ����һ���ְ���
	if (isKey) {
		std::set<std::string> needToBlock1 = { "F1", "F2", "F3", "PageUp", "PageDown" };
		if (needToBlock1.count(key.getName())) return true;
	}

	static bool needToBlockFirstUp = true;
	static bool registerNeedToClear = false;
	static bool firstMouseEvent = true;
	static bool playing = false;
	auto keyManager = KeyManager::getInstance();
	auto & currentRecordRegister = keyManager->getCurrentRecordRegister();

	if (needToBlockFirstUp) {
		needToBlockFirstUp = false;
		registerNeedToClear = true;
		return true;
	}

	// �ݹ�Ĵ���
	if (keyManager->isOnPlay()) {
		onPlay(true, mouse, key, state);
	}
	else if (isKey && keyManager->isOnRequestRegister()) {
		onRequestRegister(key, state);
		return true;
	}
	else if (isKey && keyManager->getCombinedKeyState("RWin") || keyManager->getCombinedKeyState("Menu")) {
		keyManager->setOnRequestRegister(true);
		keyManager->setOnRequestPlayRegister(true);
		// ���⴦����ͨ�������Ժ�RWin��Menuһֱ����û���ɿ�����ʱ�ٰ��¼Ĵ�����Ӧֱ�ӵ���OnRequestRegister
		if (!key.isCombinedKey())
			onRequestRegister(key, state);
		return true;
	}

	// ��������¼�Ƶİ���
	if (isKey) {
		std::set<std::string> needToBlock2 = { "F1", "F2", "F3", "F12", "RWin", "Menu", "PageUp", "PageDown" };
		if (needToBlock2.count(key.getName())) return true;
	}

	////////////////////////////////////////////////////////////////////////////////
	// ������ʼ¼��
	////////////////////////////////////////////////////////////////////////////////

	// ��������
	if (isKey) {
		auto now = system_clock::now();
		auto difft = now - keyManager->getRecordTime();
		keyManager->setRecordTime(now);
		auto duration = duration_cast<microseconds>(difft);

		currentRecordRegister.push(std::shared_ptr<SleepEvent>(new SleepEvent(duration)));

		// �����һ����ʱ
		if (registerNeedToClear) {
			currentRecordRegister.clear();
			registerNeedToClear = false;
		}

		// ����¼��
		if (!needToBlockFirstUp && key == "PrtSc" && state == DOWN) {
			needToBlockFirstUp = true;
			firstMouseEvent = true;
			keyManager->setOnRecord(false);
			currentRecordRegister.push(std::shared_ptr<StopEvent>(new StopEvent()));
			keyManager->saveRegisters();
			// ¼�ƵĹ������ڵݹ�Ĵ���
			if (keyManager->isOnPlay() || keyManager->isOnQuickPlay())
				keyManager->setUserPressStopPlay(true);
			return true;
		}

		currentRecordRegister.push(std::shared_ptr<KeyEvent>(new KeyEvent(key, state)));
		return false;
	}
	// �������
	else {
		auto mouseMode = keyManager->getMouseMode();
		auto mouseType = mouse.getMouseType();
		const int FAKE_SLEEP_TIME = 0;

		// ���ģʽ��0
		if (mouseMode == 0) return false;
		// �ų���������
		if (mouseType == Mouse::NOTYPE) return false;
		// 1��3ģʽ����¼�ƶ�
		if (mouseType == Mouse::MOUSEMOVE_ABSOLUTE && (mouseMode == 1 || mouseMode == 3)) return false;

		auto now = system_clock::now();
		auto difft = now - keyManager->getRecordTime();
		keyManager->setRecordTime(now);
		auto duration = duration_cast<microseconds>(difft);

		currentRecordRegister.push(std::shared_ptr<SleepEvent>(new SleepEvent(duration)));

		// �����һ����ʱ
		if (registerNeedToClear) {
			currentRecordRegister.clear();
			registerNeedToClear = false;
		}

		int x = mouse.getX();
		int y = mouse.getY();
		int wheelScrollDelta = mouse.getWheelScrollDelta();
		// ���ģʽ��1��2Ԥ����
		if (mouseMode == 1 || mouseMode == 2) {
			// ��¼��¼�ƹ����е�һ������¼�ʱ��λ��
			if (firstMouseEvent) {
				firstMouseEvent = false;
				POINT point;
				GetCursorPos(&point);
				keyManager->setLastX(point.x);
				keyManager->setLastY(point.y);
			}
			x -= keyManager->getLastX();
			y -= keyManager->getLastY();
		}

		// ���ƶ��¼���ֳ��ƶ�+���
		if (mouseType != Mouse::MOUSEMOVE_ABSOLUTE) {
			// �ƶ�
			if (mouseMode == 1 || mouseMode == 2) {
				currentRecordRegister.push(std::shared_ptr<MouseEvent>(new MouseEvent(Mouse(Mouse::MOUSEMOVE_RELATIVE, x, y, 0))));
				currentRecordRegister.push(std::shared_ptr<SleepEvent>(new SleepEvent(milliseconds(FAKE_SLEEP_TIME))));
			}
			else {
				currentRecordRegister.push(std::shared_ptr<MouseEvent>(new MouseEvent(Mouse(Mouse::MOUSEMOVE_ABSOLUTE, x, y, 0))));
				currentRecordRegister.push(std::shared_ptr<SleepEvent>(new SleepEvent(milliseconds(FAKE_SLEEP_TIME))));
			}
			// ���
			currentRecordRegister.push(std::shared_ptr<MouseEvent>(new MouseEvent(Mouse(mouseType, 0, 0, wheelScrollDelta))));
		}
		// �ƶ��¼��Ĵ���
		else {
			if (mouseMode == 2)
				currentRecordRegister.push(std::shared_ptr<MouseEvent>(new MouseEvent(Mouse(Mouse::MOUSEMOVE_RELATIVE, x, y, 0))));
			else
				currentRecordRegister.push(std::shared_ptr<MouseEvent>(new MouseEvent(Mouse(Mouse::MOUSEMOVE_ABSOLUTE, x, y, 0))));
			// �ƶ��¼�ֱ�Ӵ������
			return false;
		}

		// �ϲ�˫�����
		if (keyManager->getRecordDoubleClick() == 1 && mouseType == Mouse::LBUTTON_UP) {
			int bugReport = 0;
			// ˫���Ĺ����У����ٲ���15���¼�������15���¼����ж�
			auto size = currentRecordRegister.getSize();
			if (size < 15) {
				bugReport = 1;
				return false;
			}

			size_t bottomI, mi = 0;
			Mouse::MouseType mt[4] = { Mouse::LBUTTON_UP, Mouse::LBUTTON_DOWN, Mouse::LBUTTON_UP, Mouse::LBUTTON_DOWN };
			std::shared_ptr<MouseEvent> mouseEvents[4];
			std::vector<std::pair<int, int>> moveXY;
			std::vector<std::shared_ptr<SleepEvent>> sleepEvents;

			for (int i = size - 1; i >= 0; i--) {
				auto event = currentRecordRegister.getEvent(i);
				// ˫�������в������м����¼�
				if (std::dynamic_pointer_cast<KeyEvent>(event) != nullptr) {
					bugReport = 2;
					return false;
				}

				// ����¼�
				auto mouseEvent = std::dynamic_pointer_cast<MouseEvent>(event);
				if (mouseEvent != nullptr) {
					auto mouse = mouseEvent->getMouse();
					// ˫�����������¼�
					if (mouse.getMouseType() == mt[mi]) {
						mi++;
						bottomI = i;
					}
					// �������ƶ��¼�
					else if (mouse.getMouseType() == Mouse::MOUSEMOVE_ABSOLUTE) {
						moveXY.push_back(std::pair<int, int>(mouse.getX(), mouse.getY()));
					}
					// �������ƶ��¼�
					else if (mouse.getMouseType() == Mouse::MOUSEMOVE_RELATIVE) {
						moveXY.push_back(std::pair<int, int>(mouse.getX() + keyManager->getLastX(), mouse.getY() + keyManager->getLastY()));
					}
					// ��˫���������¼�
					else {
						bugReport = 3;
						return false;
					}
					continue;
				}

				// ��ʱʱ��
				auto sleepEvent = std::dynamic_pointer_cast<SleepEvent>(event);
				if (sleepEvent != nullptr)
					sleepEvents.push_back(sleepEvent);
			}

			// û����˫����4���¼�
			if (mi < 4) {
				bugReport = 4;
				return false;
			}

			// ����˫��ʱ��
			milliseconds doubleClickSleepTime(0);
			for (auto & sleepEvent : sleepEvents)
				doubleClickSleepTime += duration_cast<milliseconds>(sleepEvent->getSleepTime());
			doubleClickSleepTime -= milliseconds(4 * FAKE_SLEEP_TIME);
			// ˫�����̵�ʱ��̫��������˫��
			if (doubleClickSleepTime > milliseconds(GetDoubleClickTime())) {
				bugReport = 5;
				return false;
			}

			// ������ƫ��ֵ
			if (moveXY.size() >= 2) {
				int dx = 0, dy = 0;
				for (size_t i = 1; i < moveXY.size(); i++) {
					dx += abs(moveXY[i].first - moveXY[i - 1].first);
					dy += abs(moveXY[i].second - moveXY[i - 1].second);
				}
				// ƫ��ֵ̫����˫��
				if (dx > 10 || dy > 10) {
					bugReport = 6;
					return false;
				}

			}

			// ����˫�����������¼�
			while (currentRecordRegister.getSize() > bottomI)
				currentRecordRegister.pop();

			// ѹ��˫���¼�
			currentRecordRegister.push(std::shared_ptr<MouseEvent>(new MouseEvent(Mouse::LBUTTON_DOUBLE_CLICK)));
		}
		return false;
	}
}

// ��ͨ������
bool onPlay(int isKey, const Mouse & mouse, const Key & key, KeyEventState state) {
	static bool playing = false;
	static bool needToGetRelative = true;
	auto keyManager = KeyManager::getInstance();

	// Play״̬�У����ǻ�û��ʼ����
	if (!playing) {
		// ���ģʽ��1��2Ԥ����
		auto mouseMode = keyManager->getMouseMode();
		if (mouseMode == 1 || mouseMode == 2) {
			// ��¼��¼�ƹ����е�һ������¼�ʱ��λ��
			if (needToGetRelative) {
				needToGetRelative = false;
				POINT point;
				GetCursorPos(&point);
				keyManager->setLastX(point.x);
				keyManager->setLastY(point.y);
			}
		}
		playing = true;
		std::thread playQueue([]() {
			KeyManager::getInstance()->getCurrentPlayRegister().play();
			mutex.lock();
			KeyManager::getInstance()->setOnPlay(false);
			playing = false;
			needToGetRelative = true;
			mutex.unlock();
		});
		playQueue.detach();
		return true;
	}

	if (isKey) {
		// ���ŵ�ʱ�����F12���£�������ֹ
		if (playing && key == "F12" && state == DOWN) {
			keyManager->setUserPressStopPlay(true);
			return true;
		}

		// ���ŵ�ʱ��������Ҫ�����İ���
		std::set<std::string> needToBlock = { "F1", "F2", "F3", "F12", "PrtSc", "RWin", "Menu", "PageUp", "PageDown" };
		return needToBlock.count(key.getName()) > 0;
	}
	else {
		return false;
	}
}

// ����¼����
bool onQuickRecord(int isKey, const Mouse & mouse, const Key & key, KeyEventState state) {
	////////////////////////////////////////////////////////////////////////////////
	// ¼��ǰ׼������
	////////////////////////////////////////////////////////////////////////////////

	// ����һ���ְ���
	if (isKey) {
		std::set<std::string> needToBlock1 = { "F1", "F3", "PrtSc", "PageUp", "PageDown" };
		if (needToBlock1.count(key.getName())) return true;
	}

	static bool needToBlockFirstUp = true;
	static bool firstMouseEvent = true;
	static bool registerNeedToClear = false;
	auto keyManager = KeyManager::getInstance();
	auto & currentQuickPlayRegister = keyManager->getCurrentQuickRegister();

	if (needToBlockFirstUp) {
		needToBlockFirstUp = false;
		registerNeedToClear = true;
		return true;
	}

	// �ݹ�Ĵ���
	if (keyManager->isOnPlay()) {
		onPlay(true, Mouse(Mouse::NOTYPE), key, state);
	}
	else if (isKey && keyManager->isOnRequestRegister()) {
		onRequestRegister(key, state);
		return true;
	}
	else if (isKey && keyManager->getCombinedKeyState("RWin") || keyManager->getCombinedKeyState("Menu")) {
		keyManager->setOnRequestRegister(true);
		keyManager->setOnRequestPlayRegister(true);
		// ���⴦����ͨ�������Ժ�RWin��Menuһֱ����û���ɿ�����ʱ�ٰ��¼Ĵ�����Ӧֱ�ӵ���OnRequestRegister
		if (!key.isCombinedKey())
			onRequestRegister(key, state);
		return true;
	}

	// ��������¼�Ƶİ���
	std::set<std::string> needToBlock2 = { "F1", "F3", "F12", "PrtSc", "RWin", "Menu", "PageUp", "PageDown" };
	if (needToBlock2.count(key.getName())) return true;

	////////////////////////////////////////////////////////////////////////////////
	// ������ʼ¼��
	////////////////////////////////////////////////////////////////////////////////

	if (isKey) {
		auto now = system_clock::now();
		auto difft = now - keyManager->getRecordTime();
		keyManager->setRecordTime(now);
		auto duration = duration_cast<microseconds>(difft);

		currentQuickPlayRegister.push(std::shared_ptr<SleepEvent>(new SleepEvent(duration)));

		// �����һ����ʱ
		if (registerNeedToClear) {
			currentQuickPlayRegister.clear();
			registerNeedToClear = false;
		}

		// ����¼��
		if (!needToBlockFirstUp && key == "F2" && state == DOWN) {
			needToBlockFirstUp = true;
			firstMouseEvent = true;
			keyManager->setOnQuickRecord(false);
			currentQuickPlayRegister.push(std::shared_ptr<StopEvent>(new StopEvent()));
			keyManager->saveRegisters();
			return true;
		}

		currentQuickPlayRegister.push(std::shared_ptr<KeyEvent>(new KeyEvent(key, state)));
		return false;
	}
	else {
		auto mouseMode = keyManager->getMouseMode();
		auto mouseType = mouse.getMouseType();
		const int FAKE_SLEEP_TIME = 0;

		// ���ģʽ��0
		if (mouseMode == 0) return false;
		// �ų���������
		if (mouseType == Mouse::NOTYPE) return false;
		// 1��3ģʽ����¼�ƶ�
		if (mouseType == Mouse::MOUSEMOVE_ABSOLUTE && (mouseMode == 1 || mouseMode == 3)) return false;

		auto now = system_clock::now();
		auto difft = now - keyManager->getRecordTime();
		keyManager->setRecordTime(now);
		auto duration = duration_cast<microseconds>(difft);

		currentQuickPlayRegister.push(std::shared_ptr<SleepEvent>(new SleepEvent(duration)));

		// �����һ����ʱ
		if (registerNeedToClear) {
			currentQuickPlayRegister.clear();
			registerNeedToClear = false;
		}

		int x = mouse.getX();
		int y = mouse.getY();
		int wheelScrollDelta = mouse.getWheelScrollDelta();
		// ���ģʽ��1��2Ԥ����
		if (mouseMode == 1 || mouseMode == 2) {
			// ��¼��¼�ƹ����е�һ������¼�ʱ��λ��
			if (firstMouseEvent) {
				firstMouseEvent = false;
				POINT point;
				GetCursorPos(&point);
				keyManager->setLastX(point.x);
				keyManager->setLastY(point.y);
			}
			x -= keyManager->getLastX();
			y -= keyManager->getLastY();
		}

		// ���ƶ��¼���ֳ��ƶ�+���
		if (mouseType != Mouse::MOUSEMOVE_ABSOLUTE) {
			// �ƶ�
			if (mouseMode == 1 || mouseMode == 2) {
				currentQuickPlayRegister.push(std::shared_ptr<MouseEvent>(new MouseEvent(Mouse(Mouse::MOUSEMOVE_RELATIVE, x, y, 0))));
				currentQuickPlayRegister.push(std::shared_ptr<SleepEvent>(new SleepEvent(milliseconds(FAKE_SLEEP_TIME))));
			}
			else {
				currentQuickPlayRegister.push(std::shared_ptr<MouseEvent>(new MouseEvent(Mouse(Mouse::MOUSEMOVE_ABSOLUTE, x, y, 0))));
				currentQuickPlayRegister.push(std::shared_ptr<SleepEvent>(new SleepEvent(milliseconds(FAKE_SLEEP_TIME))));
			}
			// ���
			currentQuickPlayRegister.push(std::shared_ptr<MouseEvent>(new MouseEvent(Mouse(mouseType, 0, 0, wheelScrollDelta))));
		}
		// �ƶ��¼��Ĵ���
		else {
			if (mouseMode == 2)
				currentQuickPlayRegister.push(std::shared_ptr<MouseEvent>(new MouseEvent(Mouse(Mouse::MOUSEMOVE_RELATIVE, x, y, 0))));
			else
				currentQuickPlayRegister.push(std::shared_ptr<MouseEvent>(new MouseEvent(Mouse(Mouse::MOUSEMOVE_ABSOLUTE, x, y, 0))));
			// �ƶ��¼�ֱ�Ӵ������
			return false;
		}

		// �ϲ�˫�����
		if (keyManager->getRecordDoubleClick() == 1 && mouseType == Mouse::LBUTTON_UP) {
			int bugReport = 0;
			// ˫���Ĺ����У����ٲ���15���¼�������15���¼����ж�
			auto size = currentQuickPlayRegister.getSize();
			if (size < 15) {
				bugReport = 1;
				return false;
			}

			size_t bottomI, mi = 0;
			Mouse::MouseType mt[4] = { Mouse::LBUTTON_UP, Mouse::LBUTTON_DOWN, Mouse::LBUTTON_UP, Mouse::LBUTTON_DOWN };
			std::shared_ptr<MouseEvent> mouseEvents[4];
			std::vector<std::pair<int, int>> moveXY;
			std::vector<std::shared_ptr<SleepEvent>> sleepEvents;

			for (int i = size - 1; i >= 0; i--) {
				auto event = currentQuickPlayRegister.getEvent(i);
				// ˫�������в������м����¼�
				if (std::dynamic_pointer_cast<KeyEvent>(event) != nullptr) {
					bugReport = 2;
					return false;
				}

				// ����¼�
				auto mouseEvent = std::dynamic_pointer_cast<MouseEvent>(event);
				if (mouseEvent != nullptr) {
					auto mouse = mouseEvent->getMouse();
					// ˫�����������¼�
					if (mouse.getMouseType() == mt[mi]) {
						mi++;
						bottomI = i;
					}
					// �������ƶ��¼�
					else if (mouse.getMouseType() == Mouse::MOUSEMOVE_ABSOLUTE) {
						moveXY.push_back(std::pair<int, int>(mouse.getX(), mouse.getY()));
					}
					// �������ƶ��¼�
					else if (mouse.getMouseType() == Mouse::MOUSEMOVE_RELATIVE) {
						moveXY.push_back(std::pair<int, int>(mouse.getX() + keyManager->getLastX(), mouse.getY() + keyManager->getLastY()));
					}
					// ��˫���������¼�
					else {
						bugReport = 3;
						return false;
					}
					continue;
				}

				// ��ʱʱ��
				auto sleepEvent = std::dynamic_pointer_cast<SleepEvent>(event);
				if (sleepEvent != nullptr)
					sleepEvents.push_back(sleepEvent);
			}

			// û����˫����4���¼�
			if (mi < 4) {
				bugReport = 4;
				return false;
			}

			// ����˫��ʱ��
			milliseconds doubleClickSleepTime(0);
			for (auto & sleepEvent : sleepEvents)
				doubleClickSleepTime += duration_cast<milliseconds>(sleepEvent->getSleepTime());
			doubleClickSleepTime -= milliseconds(4 * FAKE_SLEEP_TIME);
			// ˫�����̵�ʱ��̫��������˫��
			if (doubleClickSleepTime > milliseconds(GetDoubleClickTime())) {
				bugReport = 5;
				return false;
			}

			// ������ƫ��ֵ
			if (moveXY.size() >= 2) {
				int dx = 0, dy = 0;
				for (size_t i = 1; i < moveXY.size(); i++) {
					dx += abs(moveXY[i].first - moveXY[i - 1].first);
					dy += abs(moveXY[i].second - moveXY[i - 1].second);
				}
				// ƫ��ֵ̫����˫��
				if (dx > 10 || dy > 10) {
					bugReport = 6;
					return false;
				}

			}

			// ����˫�����������¼�
			while (currentQuickPlayRegister.getSize() > bottomI)
				currentQuickPlayRegister.pop();

			// ѹ��˫���¼�
			currentQuickPlayRegister.push(std::shared_ptr<MouseEvent>(new MouseEvent(Mouse::LBUTTON_DOUBLE_CLICK)));
		}
		return false;
	}
}

// ���ٲ�����
bool onQucikPlay(int isKey, const Mouse & mouse, const Key & key, KeyEventState state) {
	static bool playing = false;
	static bool needToGetRelative = true;
	auto keyManager = KeyManager::getInstance();

	// QucikPlay״̬�У����ǻ�û��ʼ����
	if (!playing) {
		// ���ģʽ��1��2Ԥ����
		auto mouseMode = keyManager->getMouseMode();
		if (mouseMode == 1 || mouseMode == 2) {
			// ��¼��¼�ƹ����е�һ������¼�ʱ��λ��
			if (needToGetRelative) {
				needToGetRelative = false;
				POINT point;
				GetCursorPos(&point);
				keyManager->setLastX(point.x);
				keyManager->setLastY(point.y);
			}
		}
		playing = true;
		std::thread playQueue([]() {
			KeyManager::getInstance()->getCurrentQuickRegister().play();
			mutex.lock();
			KeyManager::getInstance()->setOnQucikPlay(false);
			playing = false;
			needToGetRelative = true;
			mutex.unlock();
		});
		playQueue.detach();
		return true;
	}

	if (isKey) {
		// ���ŵ�ʱ�����F12���£�������ֹ
		if (playing && key == "F12" && state == DOWN)
			keyManager->setUserPressStopPlay(true);

		// ���ŵ�ʱ��������Ҫ�����İ���
		std::set<std::string> needToBlock = { "F1", "F2", "F3", "F12", "PrtSc", "RWin", "Menu", "PageUp", "PageDown" };
		return needToBlock.count(key.getName()) > 0;
	}
	else {
		return false;
	}
}

// ����Ĵ�����
bool onRequestRegister(const Key & key, KeyEventState state) {
	auto keyManager = KeyManager::getInstance();

	if (keyManager->isOnRequestQucikRegister()) {
		if (state == DOWN) {
			std::set<std::string> needToBlock = { "F1", "F2", "F3", "F12", "PrtSc", "PageUp", "PageDown" };
			if (needToBlock.count(key.getName())) return true;
			if (key == "RWin" || key == "Menu")
				keyManager->setCurrentQuickRegister("0");
			else
				keyManager->setCurrentQuickRegister(key.getName());
			keyManager->setOnRequestQucikRegister(false);
			keyManager->setOnRequestRegister(false);
		}
	}
	else if (keyManager->isOnRequestRecordRegister()) {
		if (state == DOWN) {
			std::set<std::string> needToBlock = { "F1", "F2", "F3", "F12", "RWin", "Menu", "PageUp", "PageDown" };
			if (needToBlock.count(key.getName())) return  true;
			if (key == "PrtSc") {
				keyManager->setOnRequestRecordRegister(false);
				keyManager->setOnRequestRegister(false);
				return true;
			}
			keyManager->setOnRequestRecordRegister(false);
			keyManager->setOnRequestRegister(false);
			keyManager->setCurrentRecordRegister(key.getName());
			keyManager->setOnRecord(true);
		}
	}
	else if (keyManager->isOnRequestPlayRegister()) {
		if ((key == "RWin" || key == "Menu") && state == UP) {
			keyManager->setOnRequestPlayRegister(false);
			if (!keyManager->isOnRecord() && !keyManager->isOnQuickRecord())
				keyManager->setOnRequestQucikRegister(true);
			else
				keyManager->setOnRequestRegister(false);
		}
		else if (state == DOWN) {
			std::set<std::string> needToBlock = { "F1", "F2", "F3", "F12", "PrtSc", "RWin", "Menu", "PageUp", "PageDown" };
			if (needToBlock.count(key.getName())) return  true;
			keyManager->setCurrentPlayRegister(key.getName());
			keyManager->setOnRequestPlayRegister(false);
			keyManager->setOnRequestRegister(false);
			keyManager->setOnPlay(true);
		}
	}
	return true;
}

// ����¼��ѡ����
bool onRecordSetting(const Key & key, KeyEventState state) {
	// ������Ҫ�����İ���
	std::set<std::string> needToBlock = { "F1", "F2", "F3", "F12", "PrtSc", "RWin", "Menu", "PageDown" };
	if (needToBlock.count(key.getName())) return true;

	// �������е���
	if (state == UP) return true;

	auto keyManager = KeyManager::getInstance();
	static char mod = 0;
	static std::set<char> modeSet = { 'M', 'D' };
	static std::string settings;

	// ����
	if ((key.isAlpha() || key == "PageUp") && settings != "") {
		switch (mod) {
			case 'M': keyManager->setMouseMode(std::stoi(settings)); break;
			case 'D': keyManager->setRecordDoubleClick(std::stoi(settings)); break;
			default: break;
		}
		mod = 0;
		settings.clear();
	}

	// ����
	if (key == "PageUp") {
		keyManager->setOnRecordSetting(false);
		return true;
	}

	// �л�ģʽ
	if (key.isAlpha()) {
		char alphaChar = key.getAlphaChar();
		if (modeSet.count(alphaChar))
			mod = alphaChar;
		return true;
	}

	// ¼������
	if (key.isNumber() || key.isNumPadNumber())
		settings += key.getNumberChar();
	return true;
}

// ���ò���ѡ����
bool onPlaySetting(const Key & key, KeyEventState state) {
	// ������Ҫ�����İ���
	std::set<std::string> needToBlock = { "F1", "F2", "F3", "F12", "PrtSc", "RWin", "Menu", "PageUp" };
	if (needToBlock.count(key.getName())) return true;

	// �������е���
	if (state == UP) return true;

	auto keyManager = KeyManager::getInstance();
	static char mod = 0;
	static std::set<char> modeSet = { 'T', 'L', 'C' };
	static std::string settings;

	// ����
	if ((key.isAlpha() || key == "PageDown") && settings != "") {
		switch (mod) {
			case 'T': keyManager->setSleepTime(std::stoi(settings)); break;
			case 'L': keyManager->setLoopSleepTime(std::stoi(settings)); break;
			case 'C': keyManager->setPlayCount(std::stoi(settings)); break;
			default: break;
		}
		mod = 0;
		settings.clear();
	}

	// ����
	if (key == "PageDown") {
		keyManager->setOnPlaySetting(false);
		return true;
	}

	// �л�ģʽ
	if (key.isAlpha()) {
		char alphaChar = key.getAlphaChar();
		if (modeSet.count(alphaChar))
			mod = alphaChar;
		return true;
	}

	// ¼������
	if (key.isNumber() || key.isNumPadNumber())
		settings += key.getNumberChar();
	return true;
}

LRESULT CALLBACK parserKey(HWND hWnd, HHOOK keyboardHook, LPARAM lParam, int nCode, WPARAM wParam) {
	bool needToBlockKey = false;
	if (nCode >= 0) {
		// ��ʼ������
// 		wchar_t text[50];
// 		const wchar_t *info = NULL;
		auto keyManager = KeyManager::getInstance();
// 		HDC hdc;
		PKBDLLHOOKSTRUCT p = (PKBDLLHOOKSTRUCT)lParam;
		Key key(p->vkCode, p->scanCode);
		KeyEventState state;

		// �жϰ���״̬
		switch (wParam) {
			case WM_KEYDOWN:
				state = DOWN;
// 				info = L"��ͨ���I����";
				break;
			case WM_KEYUP:
				state = UP;
// 				info = L"��ͨ���I̧��";
				break;
			case WM_SYSKEYDOWN:
				state = DOWN;
// 				info = L"ϵ�y���I����";
				break;
			case WM_SYSKEYUP:
				state = UP;
// 				info = L"ϵ�y���I̧��";
				break;
			default:
				state = NOSTATE;
// 				info = L"δ֪����";
				break;
		}

		// ��¼��ϼ�״̬
		if (key.isCombinedKey()) {
			keyManager->processCombinedKey(key, state);
		}

		if (keyManager->isOnRecord()) {
			needToBlockKey = onRecord(true, Mouse(Mouse::NOTYPE), key, state);
		}
		else if (keyManager->isOnQuickRecord()) {
			needToBlockKey = onQuickRecord(true, Mouse(Mouse::NOTYPE), key, state);
		}
		else if (keyManager->isOnPlay()) {
			needToBlockKey = onPlay(true, Mouse(Mouse::NOTYPE), key, state);
		}
		else if (keyManager->isOnQuickPlay()) {
			needToBlockKey = onQucikPlay(true, Mouse(Mouse::NOTYPE), key, state);
		}
		else if (keyManager->isOnRequestRegister()) {
			needToBlockKey = onRequestRegister(key, state);
		}
		else if (keyManager->isOnRecordSetting()) {
			needToBlockKey = onRecordSetting(key, state);
		}
		else if (keyManager->isOnPlaySetting()) {
			needToBlockKey = onPlaySetting(key, state);
		}
		else if (key == "F2" && state == DOWN) {
			// ����¼��
			keyManager->setOnQuickRecord(true);
			needToBlockKey = true;
		}
		else if (key == "F1" && state == DOWN) {
			// ���ٲ���
			keyManager->setOnQucikPlay(true);
			needToBlockKey = true;
		}
		else if (key == "PrtSc" && state == DOWN && !keyManager->getCombinedKeyState("LWin")) {
			// ��ͨ¼��
			keyManager->setOnRequestRegister(true);
			keyManager->setOnRequestRecordRegister(true);
			needToBlockKey = true;
		}
		else if (key == "PageUp" && state == DOWN) {
			// ����¼��ѡ��
			keyManager->setOnRecordSetting(true);
			needToBlockKey = true;
		}
		else if (key == "PageDown" && state == DOWN) {
			// ���ò���ѡ��
			keyManager->setOnPlaySetting(true);
			needToBlockKey = true;
		}
		else if (keyManager->getCombinedKeyState("RWin") || keyManager->getCombinedKeyState("Menu")) {
			keyManager->setOnRequestRegister(true);
			keyManager->setOnRequestPlayRegister(true);
			needToBlockKey = true;
			// ���⴦����ͨ�������Ժ�RWin��Menuһֱ����û���ɿ�����ʱ�ٰ��¼Ĵ�����Ӧֱ�ӵ���OnRequestRegister
			if (!key.isCombinedKey())
				onRequestRegister(key, state);
		}

// 		std::wstring clearStr = L"                                                                        ";
// 		// �����Ϣ
// 		hdc = GetDC(hWnd);
// 		wsprintf(text, L"%s - ������ [%04d], ɨ���� [%04d]  ", info, key.getVkCode(), key.getScanCode());
// 		TextOut(hdc, 10, 10, clearStr.c_str(), clearStr.size());
// 		TextOut(hdc, 10, 10, text, wcslen(text));
// 		wsprintf(text, L"���IĿ��Ϊ�� %s                 ", GetWstr(key.getName()).c_str());
// 		TextOut(hdc, 10, 30, clearStr.c_str(), clearStr.size());
// 		TextOut(hdc, 10, 30, text, wcslen(text));
// 		wsprintf(text, L"�Ƿ����¼����: %s   ", keyManager->isOnQuickRecord() ? L"true" : L"false");
// 		TextOut(hdc, 10, 50, clearStr.c_str(), clearStr.size());
// 		TextOut(hdc, 10, 50, text, wcslen(text));
// 		wsprintf(text, L"�Ƿ񲥷���: %s   ", keyManager->isOnQuickPlay() ? L"true" : L"false");
// 		TextOut(hdc, 10, 70, clearStr.c_str(), clearStr.size());
// 		TextOut(hdc, 10, 70, text, wcslen(text));
// 		ReleaseDC(hWnd, hdc);
	}

	// ����Ϣ�������´���
	if (needToBlockKey)
		return true;
	else
		return CallNextHookEx(keyboardHook, nCode, wParam, lParam);
}

LRESULT CALLBACK parserMouse(HWND hWnd, HHOOK mouseHook, LPARAM lParam, int nCode, WPARAM wParam) {
	if (nCode >= 0) {
// 		HDC hdc;
// 		std::wstring text1, text2;
		auto keyManager = KeyManager::getInstance();
		POINT point = ((MSLLHOOKSTRUCT*)lParam)->pt;
		auto zDelta = (short)HIWORD(((MSLLHOOKSTRUCT*)lParam)->mouseData);
		Mouse::MouseType mouseType = Mouse::NOTYPE;

		switch (wParam) {
			case WM_LBUTTONDOWN:
// 				text1 = L"�������";
				mouseType = Mouse::LBUTTON_DOWN;
				break;
			case WM_LBUTTONUP:
// 				text1 = L"���̧��";
				mouseType = Mouse::LBUTTON_UP;
				break;
			case WM_RBUTTONDOWN:
// 				text1 = L"�Ҽ�����";
				mouseType = Mouse::RBUTTON_DOWN;
				break;
			case WM_RBUTTONUP:
// 				text1 = L"�Ҽ�̧��";
				mouseType = Mouse::RBUTTON_UP;
				break;
			case WM_MBUTTONDOWN:
// 				text1 = L"�м�����";
				mouseType = Mouse::MBUTTON_DOWN;
				break;
			case WM_MBUTTONUP:
// 				text1 = L"�м�̧��";
				mouseType = Mouse::MBUTTON_UP;
				break;
			case WM_MOUSEMOVE:
// 				text1 = L"����ƶ�";
				mouseType = Mouse::MOUSEMOVE_ABSOLUTE;
				break;
			case WM_MOUSEWHEEL:
				mouseType = Mouse::MOUSEWHEEL_SCROLL;
// 				if (zDelta > 0)
// 					text1 = L"���ֻ���";
// 				else
// 					text1 = L"���ֻ���";
// 				brea
			default:
				mouseType = Mouse::NOTYPE;
				break;
		}

		Mouse mouse(mouseType, point.x, point.y, zDelta);

		if (keyManager->isOnRecord()) {
			onRecord(false, mouse, Key(), NOSTATE);
		}
		else if (keyManager->isOnQuickRecord()) {
			onQuickRecord(false, mouse, Key(), NOSTATE);
		}
		else if (keyManager->isOnPlay()) {
			onPlay(false, mouse, Key(), NOSTATE);
		}
		else if (keyManager->isOnQuickPlay()) {
			onQucikPlay(false, mouse, Key(), NOSTATE);
		}

// 		HDC screen = GetDC(NULL);
// 		double screenX = GetDeviceCaps(screen, HORZRES);
// 		double screenY = GetDeviceCaps(screen, VERTRES);
// 		ReleaseDC(NULL, screen);
// 
// 		POINT p;
// 		GetCursorPos(&p);
// 
// 		std::wstringstream sout;
// 		sout << "(" << point.x << "," << point.y << ")" << " " << zDelta << " " << screenX << " " << screenY << " " << p.x << " " << p.y;
// 		text2 = sout.str();
// 
// 		std::wstring clearStr = L"                                                                        ";
// 		hdc = GetDC(hWnd);
// 		TextOut(hdc, 10, 90, clearStr.c_str(), clearStr.size());
// 		TextOut(hdc, 10, 90, text1.c_str(), text1.size());
// 		TextOut(hdc, 10, 110, clearStr.c_str(), clearStr.size());
// 		TextOut(hdc, 10, 110, text2.c_str(), text2.size());
// 		ReleaseDC(hWnd, hdc);
	}
	return CallNextHookEx(mouseHook, nCode, wParam, lParam);
}
