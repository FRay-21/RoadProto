using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using RoadProto.Terrain.UI.AutoCad;
using RoadProto.Terrain.UI.Bridge;
using RoadProto.Terrain.UI.Services;
using CoreApplication = Autodesk.AutoCAD.ApplicationServices.Core.Application;

namespace RoadProto.Terrain.UI;

public partial class AgentAssistantControl : UserControl
{
    private readonly AgentBackendClient client = new();
    private readonly List<AgentChatMessage> history = new();
    private AgentToolCall? pendingToolCall;
    private Border? pendingToolCard;
    private string normalStatusText = "连接本地 Agent 后端中";

    public AgentAssistantControl()
    {
        InitializeComponent();
        _ = RefreshHealthAsync();
    }

    private async Task RefreshHealthAsync()
    {
        normalStatusText = await client.IsHealthyAsync()
            ? "本地 Agent 后端已连接"
            : "本地 Agent 后端未启动";
        StatusText.Text = normalStatusText;
    }

    private async void SendButton_Click(object sender, RoutedEventArgs e)
    {
        await SendCurrentMessageAsync();
    }

    private async Task SendCurrentMessageAsync()
    {
        var text = InputBox.Text.Trim();
        if (string.IsNullOrWhiteSpace(text))
        {
            return;
        }

        InputBox.Clear();
        SetSendingState(true);

        try
        {
            var priorHistory = history.ToList();
            AddMessage("用户", text, true);
            history.Add(new AgentChatMessage { Role = "user", Content = text });

            var request = new AgentChatRequest
            {
                Message = text,
                History = priorHistory,
            };
            var response = await client.SendAsync(request);
            AddMessage("Agent", response.Reply, false);
            if (!string.IsNullOrWhiteSpace(response.Reply))
            {
                history.Add(new AgentChatMessage { Role = "assistant", Content = response.Reply });
            }

            if (response.ToolCall != null && response.RequiresConfirmation)
            {
                pendingToolCall = response.ToolCall;
                AddToolConfirmation(response.ToolCall);
            }
            else
            {
                pendingToolCall = null;
                RemovePendingToolCard();
            }
        }
        finally
        {
            SetSendingState(false);
        }
    }

    private void SetSendingState(bool isSending)
    {
        SendButton.IsEnabled = !isSending;
        StatusText.Text = isSending ? "正在请求本地 Agent 后端" : normalStatusText;
    }

    private void AddMessage(string role, string content, bool isUser)
    {
        if (string.IsNullOrWhiteSpace(content))
        {
            return;
        }

        var card = new Border
        {
            Background = (Brush)FindResource(isUser ? "UserMessageBrush" : "AgentMessageBrush"),
            BorderBrush = (Brush)FindResource("HeaderBorderBrush"),
            BorderThickness = new Thickness(1),
            CornerRadius = new CornerRadius(8),
            Padding = new Thickness(10, 8, 10, 9),
            Margin = new Thickness(isUser ? 28 : 0, 0, isUser ? 0 : 28, 10),
            Child = new StackPanel
            {
                Children =
                {
                    new TextBlock
                    {
                        Text = role,
                        Foreground = (Brush)FindResource("SecondaryTextBrush"),
                        FontSize = 11,
                        FontWeight = FontWeights.SemiBold,
                        Margin = new Thickness(0, 0, 0, 4),
                    },
                    new TextBlock
                    {
                        Text = content,
                        Foreground = (Brush)FindResource("PrimaryTextBrush"),
                        TextWrapping = TextWrapping.Wrap,
                        LineHeight = 19,
                    },
                },
            },
        };

        MessagePanel.Children.Add(card);
        ScrollToLatest();
    }

    private void AddToolConfirmation(AgentToolCall toolCall)
    {
        RemovePendingToolCard();

        var title = string.IsNullOrWhiteSpace(toolCall.ConfirmationTitle)
            ? toolCall.Tool
            : toolCall.ConfirmationTitle;
        var body = string.IsNullOrWhiteSpace(toolCall.ConfirmationBody)
            ? "请确认是否执行该 Agent 工具调用。"
            : toolCall.ConfirmationBody;
        var confirmButton = new Button
        {
            Content = "确认",
            Style = (Style)FindResource("ActionButtonStyle"),
        };
        confirmButton.Click += (_, _) => ExecutePendingTool();

        var cancelButton = new Button
        {
            Content = "取消",
            Style = (Style)FindResource("SecondaryButtonStyle"),
        };
        cancelButton.Click += (_, _) => CancelPendingTool();

        var actions = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Right,
            Margin = new Thickness(0, 10, 0, 0),
        };
        actions.Children.Add(cancelButton);
        actions.Children.Add(confirmButton);

        pendingToolCard = new Border
        {
            Background = (Brush)FindResource("CardBackgroundBrush"),
            BorderBrush = (Brush)FindResource("AccentBrush"),
            BorderThickness = new Thickness(1),
            CornerRadius = new CornerRadius(8),
            Padding = new Thickness(12),
            Margin = new Thickness(0, 0, 0, 12),
            Child = new StackPanel
            {
                Children =
                {
                    new TextBlock
                    {
                        Text = title,
                        Foreground = (Brush)FindResource("PrimaryTextBrush"),
                        FontSize = 14,
                        FontWeight = FontWeights.SemiBold,
                        TextWrapping = TextWrapping.Wrap,
                    },
                    new TextBlock
                    {
                        Text = body,
                        Foreground = (Brush)FindResource("SecondaryTextBrush"),
                        TextWrapping = TextWrapping.Wrap,
                        LineHeight = 19,
                        Margin = new Thickness(0, 8, 0, 0),
                    },
                    new TextBlock
                    {
                        Text = $"Tool: {toolCall.Tool}",
                        Foreground = (Brush)FindResource("SecondaryTextBrush"),
                        FontSize = 11,
                        TextWrapping = TextWrapping.Wrap,
                        Margin = new Thickness(0, 8, 0, 0),
                    },
                    actions,
                },
            },
        };

        MessagePanel.Children.Add(pendingToolCard);
        ScrollToLatest();
    }

    private void ExecutePendingTool()
    {
        if (pendingToolCall == null)
        {
            return;
        }

        var document = CoreApplication.DocumentManager.MdiActiveDocument;
        if (document == null)
        {
            AddMessage("Agent", "当前没有活动 CAD 文档，无法提交工具执行请求。", false);
            return;
        }

        try
        {
            var paths = AgentToolExecutionFile.WriteToolRequest(pendingToolCall);
            var requestPath = paths.RequestPath.Replace('\\', '/');
            document.SendStringToExecute($"RD_AI_EXECUTE_TOOL_FILE \"{requestPath}\"\n", true, false, true);
            AddMessage("Agent", "已提交工具执行请求，请按 CAD 命令行提示完成操作。", false);
            pendingToolCall = null;
            RemovePendingToolCard();
        }
        catch (Exception error)
        {
            AddMessage("Agent", $"提交工具执行请求失败：{error.Message}", false);
        }
    }

    private void CancelPendingTool()
    {
        pendingToolCall = null;
        RemovePendingToolCard();
        AddMessage("Agent", "已取消工具调用。", false);
    }

    private void RemovePendingToolCard()
    {
        if (pendingToolCard != null)
        {
            MessagePanel.Children.Remove(pendingToolCard);
            pendingToolCard = null;
        }
    }

    private void ScrollToLatest()
    {
        MessageScrollViewer.Dispatcher.BeginInvoke(new Action(() => MessageScrollViewer.ScrollToEnd()));
    }
}
