using System.Globalization;
using System.Windows;

namespace RoadProto.Terrain.UI;

public partial class StationLabelSettingsWindow : Window
{
    public StationLabelSettingsWindow(double interval)
    {
        InitializeComponent();
        Interval = interval > 0 ? interval : 100.0;
        IntervalTextBox.Text = Interval.ToString("G", CultureInfo.InvariantCulture);
    }

    public double Interval { get; private set; }

    private void OkButton_Click(object sender, RoutedEventArgs e)
    {
        if (!double.TryParse(IntervalTextBox.Text, NumberStyles.Float, CultureInfo.InvariantCulture, out var interval)
            || interval <= 0)
        {
            ErrorTextBlock.Text = "桩号标注间距必须大于 0。";
            return;
        }

        Interval = interval;
        DialogResult = true;
    }

    private void CancelButton_Click(object sender, RoutedEventArgs e)
    {
        DialogResult = false;
    }
}
