-- List of devices you don't want to auto-connect to (You can modify this list)
local unwantedDevices = {"WH-1000XM4"} -- Replace with actual device names

-- Global variable to store the last known "wanted" device
local lastWantedDevice = hs.audiodevice.defaultOutputDevice():name()

-- Function to check if a device is unwanted
local function isUnwantedDevice(deviceName)
    for _, unwantedDevice in ipairs(unwantedDevices) do
        if deviceName == unwantedDevice then
            return true
        end
    end
    return false
end

local function watchForAudioDeviceChanges(eventType)
    local currentDevice = hs.audiodevice.defaultOutputDevice()

    -- If the current device is unwanted and the event is not "dOut"
    if isUnwantedDevice(currentDevice:name()) and eventType ~= "dOut" then
        if lastWantedDevice then
            local device = hs.audiodevice.findOutputByName(lastWantedDevice)
            if device then
                device:setDefaultOutputDevice()
            end
        end
    else
        -- Update the lastWantedDevice if the current device is not unwanted
        -- or if it was manually selected (eventType == "dOut")
        lastWantedDevice = currentDevice:name()
    end
end

-- Initialize and start the watcher
local DeviceWatcher = hs.audiodevice.watcher
hs.audiodevice.watcher.setCallback(watchForAudioDeviceChanges)
DeviceWatcher:start()


-- ── Window Grid (KMK Keyboard) ──
-- Ctrl+Alt+Cmd+1‑8 snaps focused window to 2×4 grid on MacBook screen
--   [1] [2] [3] [4]
--   [5] [6] [7] [8]

-- Use the built-in display as the target screen
-- hs.screen.primaryScreen() returns the screen with the menu bar
local function getMacScreen()
  return hs.screen.primaryScreen()
end

local grid = {
  -- Row 1
  {x=0,    y=0,   w=0.25, h=0.5},  -- key 1: top-left
  {x=0.25, y=0,   w=0.25, h=0.5},  -- key 2: top-center-left
  {x=0.5,  y=0,   w=0.25, h=0.5},  -- key 3: top-center-right
  {x=0.75, y=0,   w=0.25, h=0.5},  -- key 4: top-right
  -- Row 2
  {x=0,    y=0.5, w=0.25, h=0.5},  -- key 5: bottom-left
  {x=0.25, y=0.5, w=0.25, h=0.5},  -- key 6: bottom-center-left
  {x=0.5,  y=0.5, w=0.25, h=0.5},  -- key 7: bottom-center-right
  {x=0.75, y=0.5, w=0.25, h=0.5},  -- key 8: bottom-right
}

local mods = {"ctrl", "alt", "cmd"}
local focusMods = {"ctrl", "alt", "cmd", "shift"}
local wf = hs.window.filter.new():setCurrentSpace(true):setDefaultFilter({})


-- ── Examine Mode State ──
-- Hold bottom-right key to examine: maximize a grid window + zoom in,
-- everything reverts when the key is released.
local examineMode = false
local savedWindowStates = {}  -- { [winId] = { frame=rect, zoomSteps=0 } }

local function findWindowAtCell(rect)
  local screen = getMacScreen()
  if not screen then return nil end
  local sf = screen:frame()
  local allWindows = wf:getWindows(hs.window.filter.sortByFocusedLast)

  local tx = sf.x + rect.x * sf.w
  local ty = sf.y + rect.y * sf.h
  local tw = rect.w * sf.w
  local th = rect.h * sf.h

  for _, win in ipairs(allWindows) do
    local f = win:frame()
    local cx = f.x + f.w / 2
    local cy = f.y + f.h / 2
    if cx >= tx and cx < tx + tw and cy >= ty and cy < ty + th then
      return win
    end
  end
  return nil
end

local function exitExamineMode()
  for winId, state in pairs(savedWindowStates) do
    local win = hs.window.get(winId)
    if win then
      -- Reverse zoom steps
      if state.zoomSteps > 0 then
        for _ = 1, state.zoomSteps do
          hs.eventtap.keyStroke({"cmd"}, "-", 0, win:application())
        end
      elseif state.zoomSteps < 0 then
        for _ = 1, -state.zoomSteps do
          hs.eventtap.keyStroke({"cmd"}, "=", 0, win:application())
        end
      end
      -- Restore original frame
      win:setFrame(state.frame, 0)
    end
  end
  savedWindowStates = {}
  examineMode = false
end

-- Ctrl+Alt+Cmd+Shift+F1 = examine mode enter (from ZMK examine_macro press)
hs.hotkey.bind(focusMods, "f1", function()
  examineMode = true
  savedWindowStates = {}
end)

-- Ctrl+Alt+Cmd+Shift+F2 = examine mode exit (from ZMK examine_macro release)
hs.hotkey.bind(focusMods, "f2", function()
  exitExamineMode()
end)

-- Ctrl+Alt+Cmd+Shift+= / - = zoom signals from left encoder in examine layer
hs.hotkey.bind(focusMods, "=", function()
  local win = hs.window.focusedWindow()
  if win and examineMode then
    local winId = win:id()
    if savedWindowStates[winId] then
      savedWindowStates[winId].zoomSteps = savedWindowStates[winId].zoomSteps + 1
    end
  end
  hs.eventtap.keyStroke({"cmd"}, "=", 0)
end)

hs.hotkey.bind(focusMods, "-", function()
  local win = hs.window.focusedWindow()
  if win and examineMode then
    local winId = win:id()
    if savedWindowStates[winId] then
      savedWindowStates[winId].zoomSteps = savedWindowStates[winId].zoomSteps - 1
    end
  end
  hs.eventtap.keyStroke({"cmd"}, "-", 0)
end)


-- ── Window Grid ──
-- Ctrl+Alt+Cmd+1‑8: move window (normal) or maximize-at-position (examine mode)

for i, rect in ipairs(grid) do
  hs.hotkey.bind(mods, tostring(i), function()
    if examineMode then
      -- Find window at this grid cell and maximize it
      local win = findWindowAtCell(rect)
      if not win then return end
      local winId = win:id()
      if not savedWindowStates[winId] then
        savedWindowStates[winId] = { frame = win:frame():copy(), zoomSteps = 0 }
      end
      win:maximize(0)
      win:focus()
    else
      -- Normal: move focused window to this grid cell
      local win = hs.window.focusedWindow()
      if not win then return end
      local screen = getMacScreen()
      if screen then
        win:moveToScreen(screen)
        win:moveToUnit(rect, 0)
      end
    end
  end)
end


-- ── Focus Window at Grid Position ──
-- Ctrl+Alt+Cmd+Shift+1‑8 focuses the window occupying that grid cell

for i, rect in ipairs(grid) do
  hs.hotkey.bind(focusMods, tostring(i), function()
    local win = findWindowAtCell(rect)
    if win then win:focus() end
  end)
end


-- ── Cycle Focus Through Grid Windows ──
-- Ctrl+Alt+Cmd+0 focuses the next window occupying a grid cell (1→2→…→8→1)

local cycleIndex = 0

hs.hotkey.bind(mods, "0", function()
  local screen = getMacScreen()
  if not screen then return end
  local sf = screen:frame()
  local allWindows = wf:getWindows(hs.window.filter.sortByFocusedLast)

  for attempt = 1, #grid do
    cycleIndex = (cycleIndex % #grid) + 1
    local r = grid[cycleIndex]

    local tx = sf.x + r.x * sf.w
    local ty = sf.y + r.y * sf.h
    local tw = r.w * sf.w
    local th = r.h * sf.h

    for _, win in ipairs(allWindows) do
      local f = win:frame()
      local cx = f.x + f.w / 2
      local cy = f.y + f.h / 2
      if cx >= tx and cx < tx + tw and cy >= ty and cy < ty + th then
        win:focus()
        return
      end
    end
  end
end)


-- ── Move to Screen 2 + Maximize (KMK Layer 1) ──
-- Ctrl+Alt+Cmd+9 moves focused window to second screen and maximizes it

hs.hotkey.bind(mods, "9", function()
  local win = hs.window.focusedWindow()
  if not win then return end
  local screens = hs.screen.allScreens()
  if #screens > 1 then
    win:moveToScreen(screens[2])
    win:maximize(0)
  end
end)


-- ── Rotary Encoder Scroll (SU120 Keyboard) ──
-- Ctrl+Alt+Cmd+Up/Down from the right encoder → mouse scroll events
local scrollAmount = 5

hs.hotkey.bind(mods, "up", function()
  hs.eventtap.scrollWheel({0, scrollAmount}, {})
end)

hs.hotkey.bind(mods, "down", function()
  hs.eventtap.scrollWheel({0, -scrollAmount}, {})
end)
