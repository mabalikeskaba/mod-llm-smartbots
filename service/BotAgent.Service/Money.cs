using System.Text;

namespace BotAgent.Service;

public static class Money
{
    // Formats a copper amount as WoW gold/silver/copper, e.g. 15500 -> "1g 55s".
    public static string Format(long copper)
    {
        if (copper <= 0)
            return "0c";

        long gold = copper / 10000;
        long silver = (copper % 10000) / 100;
        long cop = copper % 100;

        var parts = new List<string>(3);
        if (gold > 0) parts.Add($"{gold}g");
        if (silver > 0) parts.Add($"{silver}s");
        if (cop > 0 || parts.Count == 0) parts.Add($"{cop}c");

        return string.Join(' ', parts);
    }
}
