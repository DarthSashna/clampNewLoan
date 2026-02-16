#include "ChangeLoan.h"
#include "Economy/Economy.h"
#include "GameCommands/GameCommands.h"
#include "Localisation/StringIds.h"
#include "Ui/WindowManager.h"
#include "World/CompanyManager.h"

#include <algorithm>

namespace OpenLoco::GameCommands
{
    // 0x0046DE88
    static uint32_t changeLoan(const currency32_t newLoan, const uint8_t flags)
    {
        GameCommands::setExpenditureType(ExpenditureType::LoanInterest);

        const auto maxLoan = Economy::getInflationAdjustedCost(CompanyManager::getMaxLoanSize(), 0, 8) / 100 * 100;
        auto* company = CompanyManager::get(GameCommands::getUpdatingCompanyId());

        // Old saves may have a negative loan as per #3836; allow gradual increases, but not decreases.
        const currency32_t clampLow = std::min<currency32_t>(company->currentLoan, 0);
        // Sandbox mode can be used to decrease `maxLoan` such that `company->maxLoan > maxLoan`. Don't clamp the loan downwards
        // in such cases as that masks the error and betrays the owner's intention.
        const currency32_t clampHigh = std::max<currency32_t>(company->currentLoan, maxLoan);
        const currency32_t clampedNewLoan = std::clamp<currency32_t>(newLoan, clampLow, clampHigh);
        const currency32_t loanDifference = company->currentLoan - clampedNewLoan;

        if (company->currentLoan > clampedNewLoan)
        {
            if (company->cash < loanDifference)
            {
                GameCommands::setErrorText(StringIds::not_enough_cash_available);
                return FAILURE;
            }
        }
        else
        {
            if (company->currentLoan >= maxLoan)
            {
                GameCommands::setErrorText(StringIds::bank_refuses_to_increase_loan);
                return FAILURE;
            }
        }

        if (flags & Flags::apply)
        {
            company->currentLoan = clampedNewLoan;
            company->cash -= loanDifference;
            Ui::WindowManager::invalidate(Ui::WindowType::company, static_cast<uint16_t>(GameCommands::getUpdatingCompanyId()));
            if (CompanyManager::getControllingId() == GameCommands::getUpdatingCompanyId())
            {
                Ui::Windows::PlayerInfoPanel::invalidateFrame();
            }
        }
        return 0;
    }

    void changeLoan(registers& regs)
    {
        ChangeLoanArgs args(regs);
        regs.ebx = changeLoan(args.newLoan, regs.bl);
    }
}
