const presets = {
  dashscope: {
    name: "dashscope-qwen",
    baseUrl: "https://dashscope.aliyuncs.com/compatible-mode/v1",
    model: "qwen-plus"
  },
  deepseek: {
    name: "deepseek",
    baseUrl: "https://api.deepseek.com/v1",
    model: "deepseek-chat"
  },
  openai: {
    name: "openai",
    baseUrl: "https://api.openai.com/v1",
    model: "gpt-4.1"
  },
  custom: {
    name: "",
    baseUrl: "",
    model: ""
  }
};

const state = {
  profiles: [],
  selectedName: "",
  editingNew: true
};

const statusFields = {
  defaultProfile: document.getElementById("default-profile"),
  profileCount: document.getElementById("profile-count"),
  enabledSkillCount: document.getElementById("enabled-skill-count"),
  enabledKnowledgeCount: document.getElementById("enabled-knowledge-count"),
  storageRoot: document.getElementById("storage-root"),
  message: document.getElementById("admin-status-message")
};

const profileFields = {
  list: document.getElementById("profile-list"),
  form: document.getElementById("profile-form"),
  mode: document.getElementById("profile-form-mode"),
  preset: document.getElementById("profile-preset"),
  name: document.getElementById("profile-name"),
  baseUrl: document.getElementById("profile-base-url"),
  model: document.getElementById("profile-model"),
  temperature: document.getElementById("profile-temperature"),
  timeout: document.getElementById("profile-timeout"),
  apiKey: document.getElementById("profile-api-key"),
  isDefault: document.getElementById("profile-default"),
  newButton: document.getElementById("new-profile-button"),
  deleteButton: document.getElementById("delete-profile-button"),
  testButton: document.getElementById("test-profile-button"),
  refreshButton: document.getElementById("refresh-button"),
  toast: document.getElementById("toast")
};

document.addEventListener("DOMContentLoaded", () => {
  bindNavigation();
  bindProfileForm();
  refreshAll();
});

function bindNavigation() {
  const navItems = Array.from(document.querySelectorAll(".nav-item"));
  const views = Array.from(document.querySelectorAll(".view"));

  navItems.forEach((item) => {
    item.addEventListener("click", () => {
      const targetId = item.dataset.view;

      navItems.forEach((navItem) => {
        navItem.classList.toggle("is-active", navItem === item);
      });

      views.forEach((view) => {
        view.classList.toggle("is-active", view.id === targetId);
      });
    });
  });
}

function bindProfileForm() {
  profileFields.refreshButton.addEventListener("click", () => {
    refreshAll();
  });

  profileFields.newButton.addEventListener("click", () => {
    clearProfileForm();
    showToast("已切换到新建 Profile。", "is-ok");
  });

  profileFields.preset.addEventListener("change", () => {
    applyPreset(profileFields.preset.value);
  });

  profileFields.form.addEventListener("submit", async (event) => {
    event.preventDefault();
    await saveProfile();
  });

  profileFields.deleteButton.addEventListener("click", async () => {
    await deleteSelectedProfile();
  });

  profileFields.testButton.addEventListener("click", async () => {
    await testSelectedProfile();
  });
}

async function refreshAll() {
  setStatusMessage("正在读取状态");
  try {
    const [status, profiles] = await Promise.all([
      requestJson("/api/admin/status"),
      requestJson("/api/admin/model-profiles")
    ]);

    renderStatus(status);
    state.profiles = Array.isArray(profiles) ? profiles : [];
    if (!state.profiles.some((profile) => profile.name === state.selectedName)) {
      state.selectedName = state.profiles.find((profile) => profile.isDefault)?.name
        || state.profiles[0]?.name
        || "";
    }

    renderProfileList();
    if (state.selectedName) {
      selectProfile(state.selectedName);
    } else {
      clearProfileForm();
    }

    setStatusMessage("状态正常", "is-ok");
  } catch (error) {
    const message = getErrorMessage(error);
    setStatusMessage("读取失败", "is-error");
    showToast(`读取管理数据失败：${message}`, "is-error");
    renderProfileError(`无法读取 Profile：${message}`);
  }
}

function renderStatus(status) {
  statusFields.defaultProfile.textContent = status.defaultModelProfile || "未设置";
  statusFields.profileCount.textContent = formatCount(status.modelProfileCount);
  statusFields.enabledSkillCount.textContent = formatCount(status.enabledSkillCount);
  statusFields.enabledKnowledgeCount.textContent = formatCount(status.enabledKnowledgeCount);
  statusFields.storageRoot.textContent = status.storageRoot || "未配置";
}

function renderProfileList() {
  profileFields.list.textContent = "";

  if (state.profiles.length === 0) {
    const emptyState = document.createElement("div");
    emptyState.className = "empty-state";
    emptyState.textContent = "暂无模型 Profile，请新建后保存。";
    profileFields.list.appendChild(emptyState);
    return;
  }

  state.profiles.forEach((profile) => {
    const item = document.createElement("button");
    item.className = "profile-item";
    item.type = "button";
    item.classList.toggle("is-selected", profile.name === state.selectedName);
    item.addEventListener("click", () => {
      selectProfile(profile.name);
    });

    const titleRow = document.createElement("span");
    titleRow.className = "profile-item-title";

    const name = document.createElement("strong");
    name.textContent = profile.name;
    titleRow.appendChild(name);

    if (profile.isDefault) {
      const badge = document.createElement("span");
      badge.className = "badge";
      badge.textContent = "默认";
      titleRow.appendChild(badge);
    }

    const model = document.createElement("span");
    model.className = "profile-item-meta";
    model.textContent = profile.model || "未设置模型";

    const key = document.createElement("span");
    key.className = "profile-item-meta";
    key.textContent = profile.hasApiKey
      ? `Key ${profile.apiKeyMask || "已保存"}`
      : "Key 未保存";

    item.append(titleRow, model, key);
    profileFields.list.appendChild(item);
  });
}

function renderProfileError(text) {
  profileFields.list.textContent = "";
  const errorState = document.createElement("div");
  errorState.className = "empty-state is-error";
  errorState.textContent = text;
  profileFields.list.appendChild(errorState);
}

function selectProfile(name) {
  const profile = state.profiles.find((item) => item.name === name);
  if (!profile) {
    clearProfileForm();
    return;
  }

  state.selectedName = profile.name;
  state.editingNew = false;
  profileFields.mode.textContent = "编辑";
  profileFields.preset.value = detectPreset(profile);
  profileFields.name.value = profile.name;
  profileFields.baseUrl.value = profile.baseUrl || "";
  profileFields.model.value = profile.model || "";
  profileFields.temperature.value = String(profile.temperature ?? 0.2);
  profileFields.timeout.value = String(profile.timeoutSeconds || 60);
  profileFields.apiKey.value = "";
  profileFields.isDefault.checked = Boolean(profile.isDefault);
  profileFields.deleteButton.disabled = false;
  profileFields.testButton.disabled = false;
  renderProfileList();
}

function clearProfileForm() {
  state.selectedName = "";
  state.editingNew = true;
  profileFields.mode.textContent = "新建";
  profileFields.preset.value = "dashscope";
  profileFields.name.value = "";
  profileFields.baseUrl.value = "";
  profileFields.model.value = "";
  profileFields.temperature.value = "0.2";
  profileFields.timeout.value = "60";
  profileFields.apiKey.value = "";
  profileFields.isDefault.checked = state.profiles.length === 0;
  profileFields.deleteButton.disabled = true;
  profileFields.testButton.disabled = true;
  applyPreset("dashscope");
  renderProfileList();
}

function applyPreset(value) {
  const preset = presets[value] || presets.custom;
  if (value === "custom") {
    return;
  }

  if (state.editingNew || !profileFields.name.value.trim()) {
    profileFields.name.value = preset.name;
  }

  profileFields.baseUrl.value = preset.baseUrl;
  profileFields.model.value = preset.model;
}

async function saveProfile() {
  const payload = {
    name: profileFields.name.value.trim(),
    provider: "OpenAICompatible",
    baseUrl: profileFields.baseUrl.value.trim(),
    model: profileFields.model.value.trim(),
    temperature: readNumber(profileFields.temperature.value, 0.2),
    timeoutSeconds: Math.max(1, Math.trunc(readNumber(profileFields.timeout.value, 60))),
    apiKey: profileFields.apiKey.value.trim() || null,
    isDefault: profileFields.isDefault.checked
  };

  if (!payload.name) {
    showToast("Profile 名称不能为空。", "is-error");
    profileFields.name.focus();
    return;
  }

  try {
    const saved = await requestJson("/api/admin/model-profiles", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload)
    });

    showToast(`已保存 Profile：${saved.name}`, "is-ok");
    state.selectedName = saved.name;
    await refreshAll();
  } catch (error) {
    showToast(`保存失败：${getErrorMessage(error)}`, "is-error");
  }
}

async function deleteSelectedProfile() {
  const name = state.selectedName;
  if (!name) {
    showToast("请先选择要删除的 Profile。", "is-error");
    return;
  }

  try {
    await requestJson(`/api/admin/model-profiles/${encodeURIComponent(name)}`, {
      method: "DELETE"
    });

    showToast(`已删除 Profile：${name}`, "is-ok");
    state.selectedName = "";
    await refreshAll();
  } catch (error) {
    showToast(`删除失败：${getErrorMessage(error)}`, "is-error");
  }
}

async function testSelectedProfile() {
  const name = state.selectedName || profileFields.name.value.trim();
  if (!name) {
    showToast("请先选择或保存一个 Profile。", "is-error");
    return;
  }

  showToast("正在测试模型连接。");
  try {
    const result = await requestJson(`/api/admin/model-profiles/${encodeURIComponent(name)}/test`, {
      method: "POST"
    });
    showToast(`连接测试结果：${result || "无返回内容"}`, "is-ok");
  } catch (error) {
    showToast(`连接测试失败：${getErrorMessage(error)}`, "is-error");
  }
}

async function requestJson(url, options = {}) {
  const response = await fetch(url, {
    headers: { Accept: "application/json", ...(options.headers || {}) },
    ...options
  });

  if (!response.ok) {
    throw new Error(await readError(response));
  }

  if (response.status === 204) {
    return null;
  }

  const text = await response.text();
  if (!text) {
    return null;
  }

  try {
    return JSON.parse(text);
  } catch {
    return text;
  }
}

async function readError(response) {
  const text = await response.text();
  if (!text) {
    return `接口返回 ${response.status}`;
  }

  try {
    const parsed = JSON.parse(text);
    if (typeof parsed === "string") {
      return parsed;
    }

    if (parsed && typeof parsed.title === "string") {
      return parsed.title;
    }
  } catch {
    return text;
  }

  return text;
}

function detectPreset(profile) {
  const match = Object.entries(presets).find(([, preset]) => {
    return preset.baseUrl === profile.baseUrl && preset.model === profile.model;
  });
  return match ? match[0] : "custom";
}

function formatCount(value) {
  return Number.isFinite(value) ? String(value) : "0";
}

function readNumber(value, fallback) {
  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : fallback;
}

function setStatusMessage(text, className) {
  statusFields.message.textContent = text;
  statusFields.message.classList.remove("is-ok", "is-error");
  if (className) {
    statusFields.message.classList.add(className);
  }
}

function showToast(text, className) {
  profileFields.toast.hidden = false;
  profileFields.toast.textContent = text;
  profileFields.toast.classList.remove("is-ok", "is-error");
  if (className) {
    profileFields.toast.classList.add(className);
  }
}

function getErrorMessage(error) {
  return error instanceof Error ? error.message : "未知错误";
}
