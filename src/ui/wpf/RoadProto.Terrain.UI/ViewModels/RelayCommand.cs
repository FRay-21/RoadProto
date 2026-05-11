using System;
using System.Windows.Input;

namespace RoadProto.Terrain.UI.ViewModels;

public sealed class RelayCommand : ICommand
{
    private readonly Action execute_;
    private readonly Func<bool>? canExecute_;

    public RelayCommand(Action execute, Func<bool>? canExecute = null)
    {
        execute_ = execute;
        canExecute_ = canExecute;
    }

    public event EventHandler? CanExecuteChanged;

    public bool CanExecute(object? parameter) => canExecute_?.Invoke() ?? true;

    public void Execute(object? parameter) => execute_();

    public void RaiseCanExecuteChanged() => CanExecuteChanged?.Invoke(this, EventArgs.Empty);
}
