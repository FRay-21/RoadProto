const statusFields = {
  defaultProfile: document.getElementById("default-profile"),
  profileCount: document.getElementById("profile-count"),
  enabledSkillCount: document.getElementById("enabled-skill-count"),
  enabledKnowledgeCount: document.getElementById("enabled-knowledge-count"),
  storageRoot: document.getElementById("storage-root"),
  message: document.getElementById("admin-status-message")
};

document.addEventListener("DOMContentLoaded", () => {
  bindNavigation();
  loadAdminStatus();
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

async function loadAdminStatus() {
  setStatusMessage("正在读取状态");

  try {
    const response = await fetch("/api/admin/status", {
      headers: { Accept: "application/json" }
    });

    if (!response.ok) {
      throw new Error(`状态接口返回 ${response.status}`);
    }

    const status = await response.json();
    statusFields.defaultProfile.textContent = status.defaultModelProfile || "未设置";
    statusFields.profileCount.textContent = formatCount(status.modelProfileCount);
    statusFields.enabledSkillCount.textContent = formatCount(status.enabledSkillCount);
    statusFields.enabledKnowledgeCount.textContent = formatCount(status.enabledKnowledgeCount);
    statusFields.storageRoot.textContent = status.storageRoot || "未配置";
    setStatusMessage("状态正常", "is-ok");
  } catch (error) {
    const message = error instanceof Error ? error.message : "未知错误";
    setStatusMessage("读取失败", "is-error");
    showStatusError(`无法读取管理状态：${message}`);
  }
}

function formatCount(value) {
  return Number.isFinite(value) ? String(value) : "0";
}

function setStatusMessage(text, className) {
  statusFields.message.textContent = text;
  statusFields.message.classList.remove("is-ok", "is-error");
  if (className) {
    statusFields.message.classList.add(className);
  }
}

function showStatusError(text) {
  const profileList = document.getElementById("profile-list");
  if (!profileList) {
    return;
  }

  profileList.innerHTML = "";
  const errorState = document.createElement("div");
  errorState.className = "empty-state is-error";
  errorState.textContent = text;
  profileList.appendChild(errorState);
}
